#include "glibc_compat.h"
#include "pg_stub.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <apiClient.h>
#include <CoreV1API.h>
#include <incluster_config.h>
#include <list.h>
#include <keyValuePair.h>

// some pg magic value to indicate that it's a plugin
PG_MODULE_MAGIC;

typedef struct {
	struct in_addr network;
	struct in_addr mask;
	bool valid;
} cidr_config_t;

static ClientAuthentication_hook_type prev_client_auth_hook = NULL;
static bool cache_initialized = false;
static char* cached_instance_id = NULL;
static char* cached_pod_cidr_str = NULL;
static cidr_config_t cached_cidr = { .valid = false };
static apiClient_t* cached_api_client = NULL;

// is required to to fix linking issues (no-stack-protector)
void __stack_chk_fail(void) {
	fprintf(stderr, "FATAL: stack smashing detected in auth_k8s\n");
	exit(1);
}

static bool is_ip_in_cidr(const char* ip, const cidr_config_t* config) {
	struct in_addr addr;
	if (!config->valid || inet_aton(ip, &addr) == 0) {
		return false;
	}

	return (addr.s_addr & config->mask.s_addr) == (config->network.s_addr & config->mask.s_addr);
}

static void
auth_k8s_hook(Port* port, int status)
{
	const char* env_inst = getenv("POSTGRES_K8S_INSTANCE_ID");
	const char* env_cidr = getenv("POSTGRES_K8S_POD_CIDR");
	bool authorized = false;
	char* matched_pod_name = NULL;

	if (!cache_initialized) {
		// switching to long-term postgres context goes here
		MemoryContext oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		cached_instance_id = (env_inst != NULL) ? pstrdup(env_inst) : NULL;

		{
			if (env_cidr != NULL) {
				char* network_addr = pstrdup(env_cidr);
				// pointer to slash
				char* slash = strchr(network_addr, '/');
				if (slash != NULL) {
					*slash = '\0';
					int network_bits = atoi(slash + 1);
					if (network_bits >= 0 && network_bits <= 32 && inet_aton(network_addr, &cached_cidr.network) != 0) {
                                                uint32_t host_part_start = (unsigned int)1 << (32 - network_bits);
						uint32_t host_mask = host_part_start - 1;
						uint32_t network_mask_cpu = ~host_mask;
						cached_cidr.mask.s_addr = (network_bits == 0) ? 0 : htonl(network_mask_cpu);

						cached_cidr.valid = true;
						cached_pod_cidr_str = pstrdup(network_addr);
					}
				}
				pfree(network_addr);
			}

			char* basePath = NULL;
			sslConfig_t* sslConfig = NULL;
			list_t* apiKeys = NULL;
			if (load_incluster_config(&basePath, &sslConfig, &apiKeys) == 0) {
				cached_api_client = apiClient_create_with_base_path(basePath, sslConfig, apiKeys);
			}
		}

		cache_initialized = true;
		// switching back to short-term context
		MemoryContextSwitchTo(oldcontext);
	}

	if (cached_instance_id == NULL) {
		ereport(FATAL, (errmsg("auth Hook: POSTGRES_K8S_INSTANCE_ID is not set")));
	}

	if (cached_pod_cidr_str == NULL || !cached_cidr.valid) {
		ereport(FATAL, (errmsg("auth Hook: POSTGRES_K8S_POD_CIDR is not set or invalid")));
	}

	if (cached_api_client != NULL && port->remote_host != NULL && strlen(port->remote_host) > 0) {
		if (!is_ip_in_cidr(port->remote_host, &cached_cidr)) {
			ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("instance %s denied: IP %s is outside of allowed POD CIDR %s", cached_instance_id, port->remote_host, cached_pod_cidr_str)));
		}

		v1_pod_list_t* pod_list = NULL;
		int* i_null = NULL;
		char* resourceVersion = "0"; 

		// get all pods from the cluster ( from cache )
		pod_list = CoreV1API_listPodForAllNamespaces(cached_api_client, i_null, NULL, NULL, NULL, i_null, resourceVersion, NULL, NULL, i_null, i_null, i_null);
		if (pod_list == NULL) {
			if (cached_api_client->response_code == 403) {
				ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("instance %s denied: rbac error (403) - not enough permissions", cached_instance_id)));
			} else {
				ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("instance %s denied: k8s api error (%ld).", cached_instance_id, cached_api_client->response_code)));
			}
		} else if (pod_list->items != NULL) {
			listEntry_t* listEntry = NULL;
			v1_pod_t* matched_pod = NULL;

			list_ForEach(listEntry, pod_list->items)
			{
				v1_pod_t* pod = (v1_pod_t*)listEntry->data;
				if (pod->status != NULL && pod->status->pod_ip != NULL && strcmp(pod->status->pod_ip, port->remote_host) == 0) {
					matched_pod = pod;
					break;
				}
			}

			if (matched_pod != NULL && matched_pod->metadata != NULL) {
				matched_pod_name = matched_pod->metadata->name;

				if (matched_pod->metadata->annotations != NULL) {
					char* target_key = psprintf("otus.project.postgres.instance/%s", cached_instance_id);

					listEntry_t* annEntry = NULL;
					list_ForEach(annEntry, matched_pod->metadata->annotations)
					{
						keyValuePair_t* kv = (keyValuePair_t* )annEntry->data;
						if (kv->key != NULL && strcmp(kv->key, target_key) == 0) {
							if (kv->value != NULL && strcmp((char* )kv->value, "true") == 0) {
								authorized = true;
							}
							break;
						}
					}
					pfree(target_key);
				}
			}
			
			if (authorized) {
				ereport(LOG, (errmsg("instance %s authorized: connection from pod %s (ip: %s) verified via annotation", cached_instance_id, matched_pod_name ? matched_pod_name : "unknown", port->remote_host)));
			} else {
				// using standard error code 28000
				ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("instance %s denied: pod %s found for ip %s, but valid annotation is missing", cached_instance_id, matched_pod_name ? matched_pod_name : "unknown", port->remote_host)));
			}

			v1_pod_list_free(pod_list);
		}
	} else if (port->remote_host == NULL || strlen(port->remote_host) == 0) {
		// localhost connection
		authorized = true;
	} else {
		ereport(FATAL, (errmsg("instance %s denied: k8s api client not initialized", cached_instance_id)));
	}

	// call next auth hook if any
	if (authorized && prev_client_auth_hook != NULL) {
		prev_client_auth_hook(port, status);
	}
}

void _PG_init(void) {
	prev_client_auth_hook = ClientAuthentication_hook;
	ClientAuthentication_hook = auth_k8s_hook;
}
