#include <stdio.h>
#include <stdlib.h>
const char *empty_str = "";
const char *int_format = "%ld ";
int data[] = {4, 8, 15, 16, 23, 42};
int data_length = sizeof(data)/sizeof(data[0]);

struct ListElement {
	int value;
	struct ListElement *next;
};

void print_int(int arg) {
	printf(int_format, arg);
}

int p(int arg) {
	return arg % 2;
}

struct ListElement *add_element(int value, struct ListElement *next){
	struct ListElement *new_element = (struct ListElement *)malloc(sizeof(struct ListElement));
	if (!new_element) {
		return new_element;
	}

	new_element->value = value;
	new_element->next = next;

	return new_element;
}

void m(struct ListElement *element, void(*func)(int)) {
	if (!element) {
		return;
	}

	func(element->value);
	m(element->next, func);
}

struct ListElement *f(struct ListElement *element, struct ListElement *head, int(*func)(int)) {
	if (!element) {
		return head;
	}

	int res = func(element->value);
	if (res) {
		head = add_element(element->value, head);
	}

	head = f(element->next, head, func);

	free(element);

	return head;
}

int main() {
	int i;
	struct ListElement *head;
	for (i=data_length-1;i>=0;i--) {
		head = add_element(data[i], head);
		if (!head) {
			printf("Failed to allocate memory for a new element of the list\n");
			return 1;
		}
	}

	m(head, print_int);
	printf("%s\n", empty_str);

	struct ListElement *new_head = f(head, NULL, p);
	m(new_head, print_int);
	printf("%s\n", empty_str);

	while (1) {
		struct ListElement *p = new_head->next;
		free(new_head);
		if (!p) {
			break;
		}
		new_head = p;
	}

	return 0;
}
