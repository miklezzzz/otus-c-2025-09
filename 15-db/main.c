#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <sqlite3.h>

void run_postgres(int argc, char* argv[]);
void run_sqlite(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage (postgres): %s postgres <host> <port> <dbname> <table> <user> <password> <column>\n", argv[0]);
		fprintf(stderr, "usage (sqlite):   %s sqlite <db_path> <table> <column>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "postgres") == 0) {
		run_postgres(argc, argv);
	} else if (strcmp(argv[1], "sqlite") == 0) {
		run_sqlite(argc, argv);
	} else {
		fprintf(stderr, "use 'postgres' or 'sqlite'.\n");
		return 1;
	}

	return 0;
}

void run_postgres(int argc, char* argv[]) {
	if (argc != 9) {
		fprintf(stderr, "postgres usage: %s postgres <host> <port> <dbname> <table> <user> <password> <column>\n", argv[0]);
		exit(1);
	}

	char conninfo[1024];
	// Postgres indices: host=2, port=3, dbname=4, table=5, user=6, pass=7, col=8
	snprintf(conninfo, sizeof(conninfo), "host=%s port=%s dbname=%s user=%s password=%s connect_timeout=5", 
			 argv[2], argv[3], argv[4], argv[6], argv[7]);

	PGconn* conn = PQconnectdb(conninfo);
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "failed to connect to the postgres db: %s", PQerrorMessage(conn));
		PQfinish(conn);
	       	exit(1);
	}

	char check_q[512];
	snprintf(check_q, sizeof(check_q), "SELECT %s FROM %s LIMIT 0", argv[8], argv[5]);
	PGresult* check_res = PQexec(conn, check_q);
	
	if (PQresultStatus(check_res) == PGRES_TUPLES_OK) {
		Oid type = PQftype(check_res, 0);
		if (!(type == 20 || type == 21 || type == 23 || type == 700 || type == 701 || type == 1700)) {
			fprintf(stderr, "column '%s' is not numeric.\n", argv[8]);
			PQclear(check_res);
		       	PQfinish(conn);
		       	exit(1);
		}
	} else {
		fprintf(stderr, "postgres schema error: %s", PQerrorMessage(conn));
		PQclear(check_res);
	       	PQfinish(conn);
	       	exit(1);
	}
	PQclear(check_res);

	char query[1024];
	snprintf(query, sizeof(query), "SELECT AVG(%s), MAX(%s), MIN(%s), SUM(%s), var_pop(%s) FROM %s", 
			 argv[8], argv[8], argv[8], argv[8], argv[8], argv[5]);
	
	PGresult* res = PQexec(conn, query);
	printf("Avg: %s | Max: %s | Min: %s | Sum: %s | Dispersion: %s\n", 
		   PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1), PQgetvalue(res, 0, 2), PQgetvalue(res, 0, 3), PQgetvalue(res, 0, 4));
	
	PQclear(res); 
	PQfinish(conn);
}

void run_sqlite(int argc, char* argv[]) {
	if (argc != 5) {
		fprintf(stderr, "sqlite usage: %s sqlite <db_path> <table> <column>\n", argv[0]);
		exit(1);
	}

	sqlite3* db;
	sqlite3_stmt* stmt;
	if (sqlite3_open(argv[2], &db) != SQLITE_OK) {
		fprintf(stderr, "failed to connect to the sqlite db: %s\n", sqlite3_errmsg(db));
		exit(1);
	}

	char check_q[512];
	snprintf(check_q, sizeof(check_q), "SELECT %s FROM %s LIMIT 1", argv[4], argv[3]);

	if (sqlite3_prepare_v2(db, check_q, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "sqlite schema error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
	       	exit(1);
	}
	
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int type = sqlite3_column_type(stmt, 0);
		if (type != SQLITE_INTEGER && type != SQLITE_FLOAT && type != SQLITE_NULL) {
			fprintf(stderr, "column '%s' is not numeric.\n", argv[4]);
			sqlite3_finalize(stmt);
		sqlite3_close(db);
		exit(1);
		}
	}
	sqlite3_finalize(stmt);

	char query[1024];
	snprintf(query, sizeof(query), "SELECT AVG(%s), MAX(%s), MIN(%s), SUM(%s), (AVG(%s*%s) - (AVG(%s)*AVG(%s))) FROM %s", 
			 argv[4], argv[4], argv[4], argv[4], argv[4], argv[4], argv[4], argv[4], argv[3]);

	if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			printf("Avg: %s | Max: %s | Min: %s | Sum: %s | Dispersion: %s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4));
		}
	}

	sqlite3_finalize(stmt); 
	sqlite3_close(db);
}

