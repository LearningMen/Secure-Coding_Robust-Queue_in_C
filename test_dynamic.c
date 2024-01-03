#include <stdio.h>
#include "fqlib_dynamic.h"

void report_test(const char* test_name, int condition) {
    if (condition) {
        printf("%s: Test Pass\n", test_name);
    } else {
        printf("%s: Test Fail\n", test_name);
    }
    printf("Current error status is: %s \n", qe_errbuf);
    printf("\n\n");
}

void test_queue_operations() {
    QTICKET q = create_queue();
    int result = QE_NONE;
    int expected, actual;

    // Test adding and removing elements (FIFO order)
    for (int i = 0; i < 5; i++) {
        result = put_on_queue(q, i);
        if (result != QE_NONE) {
            report_test("Adding to Queue", 0);
            return;
        }
    }
    for (int i = 0; i < 5; i++) {
        expected = i;
        actual = take_off_queue(q);
        if (actual != expected) {
            report_test("Removing from Queue (FIFO order)", 0);
            return;
        }
    }
    report_test("Adding and Removing from Queue (FIFO order)", 1);

    // Test adding elements until the queue is full
    for (int i = 0; i < INITIAL_QUE_SIZE; i++) {
        result = put_on_queue(q, i);
        if (result != QE_NONE) {
            report_test("Adding to Full Queue", 0);
            return;
        }
    }
    result = put_on_queue(q, 99); // Attempt to add one more element
    report_test("Adding to Full Queue and check expansion", result == QE_NONE);

    // Test removing elements from an empty queue
    for (int i = 0; i < INITIAL_QUE_SIZE; i++) {
        actual = take_off_queue(q);
        if (actual < 0) {
            report_test("Removing from Empty Queue", 0);
            return;
        }
    }
    actual = take_off_queue(q); // Attempt to remove from an empty queue
    report_test("Removing from Empty Queue", actual == QE_EMPTY);
    reset_error_status();

    // Test mixed adding and removing operations
    result = put_on_queue(q, 10) == QE_NONE &&
             take_off_queue(q) == 10 &&
             put_on_queue(q, 20) == QE_NONE &&
             take_off_queue(q) == 20;
    report_test("Mixed Adding and Removing Operations", result);

    // Clean up
    delete_queue(q);
    reset_queues();
}

int main() {
    test_queue_operations();
    return 0;
}