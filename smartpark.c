/*
 * SmartPark: A Smart Parking Management System
 * Built in C on an Array, a Circular Queue and a Stack (CGI backend)
 *
 * Build: gcc -Wall -Wextra smartpark.c -o smartpark.cgi
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SLOT_COUNT 12
#define QUEUE_CAPACITY 100
#define VEHICLE_LEN 24
#define HISTORY_CAPACITY 20
#define DATA_FILE "smartpark.dat"
#define HISTORY_FILE "smartpark_history.dat"

typedef struct {
    int slot_id;
    char vehicle[VEHICLE_LEN];
    int occupied;
} Slot;

typedef struct {
    char items[QUEUE_CAPACITY][VEHICLE_LEN];
    int front, rear, count;
} WaitingQueue;

typedef struct {
    Slot slots[SLOT_COUNT];
    WaitingQueue waiting;
} ParkingState;

typedef struct {
    ParkingState items[HISTORY_CAPACITY];
    int count;
} HistoryStack;

static ParkingState state;
static HistoryStack history;

/* ---------- Initialisation and file storage ---------- */

void initialize(void) {
    int i;
    memset(&state, 0, sizeof(state));
    for (i = 0; i < SLOT_COUNT; i++) {
        state.slots[i].slot_id = i + 1;
        state.slots[i].occupied = 0;
    }
    state.waiting.front = 0;
    state.waiting.rear = -1;
    state.waiting.count = 0;
    history.count = 0;
}

int load_state(void) {
    FILE *fp = fopen(DATA_FILE, "rb");
    if (!fp) return 0;
    if (fread(&state, sizeof(state), 1, fp) != 1) {
        fclose(fp);
        initialize();
        return 0;
    }
    fclose(fp);
    return 1;
}

int save_state(void) {
    FILE *fp = fopen(DATA_FILE, "wb");
    if (!fp) return 0;
    if (fwrite(&state, sizeof(state), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

/* ---------- Stack-based undo history ---------- */

void load_history(void) {
    FILE *fp = fopen(HISTORY_FILE, "rb");
    if (!fp) { history.count = 0; return; }
    if (fread(&history, sizeof(history), 1, fp) != 1)
        history.count = 0;
    fclose(fp);
}

void save_history(void) {
    FILE *fp = fopen(HISTORY_FILE, "wb");
    if (!fp) return;
    fwrite(&history, sizeof(history), 1, fp);
    fclose(fp);
}

void push_history(void) {
    if (history.count == HISTORY_CAPACITY) {
        memmove(&history.items[0], &history.items[1],
                sizeof(ParkingState) * (HISTORY_CAPACITY - 1));
        history.count--;
    }
    history.items[history.count++] = state; /* push a snapshot */
    save_history();
}

int undo_last_change(void) {
    if (history.count <= 0) return 0;
    state = history.items[--history.count]; /* pop and restore */
    save_history();
    return save_state();
}

/* ---------- Circular queue and vehicle search ---------- */

int enqueue(const char *vehicle) {
    WaitingQueue *q = &state.waiting;
    int pos;
    if (q->count == QUEUE_CAPACITY) return 0;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    pos = q->rear;
    snprintf(q->items[pos], VEHICLE_LEN, "%s", vehicle);
    q->count++;
    return 1;
}

int dequeue(char *vehicle) {
    WaitingQueue *q = &state.waiting;
    if (q->count == 0) return 0;
    snprintf(vehicle, VEHICLE_LEN, "%s", q->items[q->front]);
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->count--;
    return 1;
}

int find_vehicle(const char *vehicle) {
    int i;
    for (i = 0; i < SLOT_COUNT; i++)
        if (state.slots[i].occupied &&
            strcmp(state.slots[i].vehicle, vehicle) == 0)
            return i;
    return -1;
}

int vehicle_is_waiting(const char *vehicle) {
    int i, pos;
    WaitingQueue *q = &state.waiting;
    for (i = 0; i < q->count; i++) {
        pos = (q->front + i) % QUEUE_CAPACITY;
        if (strcmp(q->items[pos], vehicle) == 0) return 1;
    }
    return 0;
}

/* ---------- Booking and release ---------- */

int book_vehicle(const char *vehicle, char *message, size_t size) {
    int i;
    if (find_vehicle(vehicle) >= 0 || vehicle_is_waiting(vehicle)) {
        snprintf(message, size, "This vehicle already has an active request.");
        return 0;
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        if (!state.slots[i].occupied) {
            state.slots[i].occupied = 1;
            snprintf(state.slots[i].vehicle, VEHICLE_LEN, "%s", vehicle);
            snprintf(message, size, "Vehicle booked in slot %d.",
                     state.slots[i].slot_id);
            return 1;
        }
    }
    if (enqueue(vehicle)) {
        snprintf(message, size,
                 "Parking is full. Vehicle added to waiting queue.");
        return 1;
    }
    snprintf(message, size, "Waiting queue is full. Request was not added.");
    return 0;
}

int release_vehicle(const char *vehicle, char *message, size_t size) {
    int index = find_vehicle(vehicle);
    char next[VEHICLE_LEN];
    if (index < 0) {
        snprintf(message, size, "Vehicle was not found in an occupied slot.");
        return 0;
    }
    state.slots[index].occupied = 0;
    state.slots[index].vehicle[0] = '\0';
    if (dequeue(next)) { /* oldest waiting vehicle */
        state.slots[index].occupied = 1;
        snprintf(state.slots[index].vehicle, VEHICLE_LEN, "%s", next);
        snprintf(message, size,
                 "Slot %d released and assigned to waiting vehicle %s.",
                 state.slots[index].slot_id, next);
    } else {
        snprintf(message, size, "Vehicle released. Slot %d is now empty.",
                 state.slots[index].slot_id);
    }
    return 1;
}

/* ---------- CGI input handling ---------- */

/* Decode application/x-www-form-urlencoded data. */
void url_decode(char *s) {
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '+') { *dst++ = ' '; src++; }
        else if (*src == '%' && isxdigit((unsigned char)src[1]) &&
                 isxdigit((unsigned char)src[2])) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void get_field(const char *data, const char *name,
               char *out, size_t out_size) {
    char copy[4096], *token, *eq;
    out[0] = '\0';
    snprintf(copy, sizeof(copy), "%s", data ? data : "");
    token = strtok(copy, "&");
    while (token) {
        eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(token, name) == 0) {
                snprintf(out, out_size, "%s", eq + 1);
                url_decode(out);
                return;
            }
        }
        token = strtok(NULL, "&");
    }
}

void normalize_vehicle(char *vehicle) {
    size_t i, start = 0, end = strlen(vehicle);
    while (start < end && isspace((unsigned char)vehicle[start])) start++;
    while (end > start && isspace((unsigned char)vehicle[end - 1])) end--;
    memmove(vehicle, vehicle + start, end - start);
    vehicle[end - start] = '\0';
    for (i = 0; vehicle[i]; i++)
        vehicle[i] = (char)toupper((unsigned char)vehicle[i]);
}

/* ---------- HTML response generation ---------- */

void html_escape(const char *src) {
    for (; *src; src++) {
        switch (*src) {
            case '&':  printf("&amp;");  break;
            case '<':  printf("&lt;");   break;
            case '>':  printf("&gt;");   break;
            case '"':  printf("&quot;"); break;
            case '\'': printf("&#39;");  break;
            default:   putchar(*src);
        }
    }
}

void show_page(const char *message) {
    int i;
    printf("Content-Type: text/html; charset=UTF-8\r\n\r\n");
    printf("<!DOCTYPE html><html><head><title>SmartPark Result</title>");
    printf("<meta charset='UTF-8'></head><body>");
    printf("<h1>SmartPark Parking Status</h1><p>");
    html_escape(message);
    printf("</p><table border='1'><tr><th>Slot</th><th>Status</th>");
    printf("<th>Vehicle</th></tr>");
    for (i = 0; i < SLOT_COUNT; i++) {
        printf("<tr><td>%d</td><td>%s</td><td>",
               state.slots[i].slot_id,
               state.slots[i].occupied ? "Occupied" : "Available");
        if (state.slots[i].occupied)
            html_escape(state.slots[i].vehicle);
        else
            printf("-");
        printf("</td></tr>");
    }
    printf("</table><p>Waiting vehicles: %d</p>", state.waiting.count);
    printf("<p><a href='/smartpark/index.html'>Back to SmartPark</a></p>");
    printf("</body></html>");
}

/* ---------- CGI entry point and dispatcher ---------- */

int main(void) {
    char *method = getenv("REQUEST_METHOD");
    char *length_text = getenv("CONTENT_LENGTH");
    int length = length_text ? atoi(length_text) : 0;
    char input[4096] = "";
    char action[32] = "";
    char vehicle[VEHICLE_LEN] = "";
    char message[256] = "Parking status loaded.";

    if (!load_state()) initialize();
    load_history();

    if (method && strcmp(method, "POST") == 0 &&
        length > 0 && length < (int)sizeof(input)) {
        size_t n = fread(input, 1, (size_t)length, stdin);
        input[n] = '\0';
    }

    get_field(input, "action", action, sizeof(action));
    get_field(input, "vehicle", vehicle, sizeof(vehicle));
    normalize_vehicle(vehicle);

    if (strcmp(action, "book") == 0) {
        if (vehicle[0] == '\0') {
            snprintf(message, sizeof(message), "Enter a valid vehicle number.");
        } else {
            push_history();
            if (!book_vehicle(vehicle, message, sizeof(message))) {
                /* Remove the unused snapshot when no state changed. */
                if (history.count > 0) history.count--;
                save_history();
            } else if (!save_state()) {
                snprintf(message, sizeof(message), "Could not save parking data.");
            }
        }
    } else if (strcmp(action, "release") == 0) {
        if (vehicle[0] == '\0') {
            snprintf(message, sizeof(message), "Enter a vehicle number.");
        } else if (find_vehicle(vehicle) < 0) {
            snprintf(message, sizeof(message), "Vehicle was not found.");
        } else {
            push_history();
            release_vehicle(vehicle, message, sizeof(message));
            if (!save_state())
                snprintf(message, sizeof(message), "Could not save parking data.");
        }
    } else if (strcmp(action, "search") == 0) {
        int found = find_vehicle(vehicle);
        if (found >= 0)
            snprintf(message, sizeof(message), "Vehicle found in slot %d.",
                     state.slots[found].slot_id);
        else if (vehicle_is_waiting(vehicle))
            snprintf(message, sizeof(message), "Vehicle is in the waiting queue.");
        else
            snprintf(message, sizeof(message), "Vehicle was not found.");
    } else if (strcmp(action, "undo") == 0) {
        snprintf(message, sizeof(message),
                 undo_last_change() ? "Previous state restored." :
                                      "No previous state is available.");
    } else if (strcmp(action, "display") == 0 || action[0] == '\0') {
        snprintf(message, sizeof(message), "Current parking status.");
    } else {
        snprintf(message, sizeof(message), "Unknown action.");
    }

    show_page(message);
    return 0;
}
