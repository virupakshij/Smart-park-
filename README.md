# SmartPark: A Smart Parking Management System

A parking manager for a 12-slot parking area, written in **C** and built on an **array**, a **circular queue** and a **stack**. The browser only shows HTML forms; every rule lives in a compiled C program that runs as a **CGI** backend.

> Data Structures (25CSE201) project, Dept. of Information Science and Engineering, Nitte Meenakshi Institute of Technology (NMIT), Bangalore. Academic year 2026-2027.

---

## Features

- **Book a slot**: a valid vehicle gets the first free slot (slot 1 to 12).
- **Waiting queue**: when all slots are full, the vehicle joins a FIFO queue (capacity 100).
- **Fair release**: when a vehicle leaves, its slot goes automatically to the vehicle that has waited longest.
- **Search**: find a vehicle in a slot or in the waiting queue.
- **Undo**: take back the latest booking or release (up to 20 levels).
- **Persistence**: state and undo history are saved to local binary files after every change.
- **Validation**: identifiers are trimmed, upper-cased, limited to 23 characters, and duplicates are rejected.
- **Safe output**: all user text is HTML-escaped before printing.

## Data Structures Used

| Structure | Used for | Cost |
|---|---|---|
| Array of `Slot` | The 12 parking slots, accessed by index | O(1) |
| Circular queue (FIFO) | Waiting vehicles (`rear = (rear+1) % 100`) | O(1) enqueue/dequeue |
| Stack (LIFO) | Snapshots of `ParkingState` for undo | O(1) push/pop + snapshot copy |
| Linear search | Locating a vehicle (`strcmp` over slots, walk over queue) | O(n + q) |
| File handling | Persisting state between CGI requests | proportional to data |

Core types (`smartpark.c`): `Slot`, `WaitingQueue`, `ParkingState`, `HistoryStack`.

## Architecture

```
Browser (index.html + style.css)
        |  form POST (action, vehicle)
        v
CGI interface (stdin, REQUEST_METHOD, CONTENT_LENGTH)
        |
        v
C backend (smartpark.c): validate, book, release, search, undo, show_page
        |
        v
Data: arrays + queue + stack in memory
      smartpark.dat, smartpark_history.dat on disk
```

A CGI program starts fresh on every request, so each request loads the state, runs one action, saves if the state changed, and prints the HTML page.

## Project Structure

```
smartpark/
├── smartpark.c            # C backend (CGI program)
├── index.html             # Front-end forms
├── style.css              # Styling
├── smartpark.dat          # Created at run time: slots + queue
├── smartpark_history.dat  # Created at run time: undo snapshots
└── README.md
```

## Requirements

- GCC or Clang (standard C)
- A CGI-capable local web server (e.g. Apache) on Windows, Linux or macOS
- A browser

## Build

```bash
gcc -Wall -Wextra smartpark.c -o smartpark.cgi
```

## Run

1. Place `smartpark.cgi` in the web server's CGI directory with execute permission.
2. Place `index.html` and `style.css` in the server's document directory. The form `action` must match the CGI URL.
3. Make sure the CGI program can write `smartpark.dat` and `smartpark_history.dat` in its working directory.
4. Open `index.html` in the browser.

## Quick Test Without a Web Server (Linux/macOS/MSYS2)

```bash
data='action=book&vehicle=KA01AB1234'
echo -n "$data" | REQUEST_METHOD=POST CONTENT_LENGTH=${#data} ./smartpark.cgi

data='action=search&vehicle=KA01AB1234'
echo -n "$data" | REQUEST_METHOD=POST CONTENT_LENGTH=${#data} ./smartpark.cgi
```

## Request Interface

POST form fields (`application/x-www-form-urlencoded`, max 4095 bytes):

| `action` | `vehicle` | Result |
|---|---|---|
| `book` | required | Books a slot or joins the queue |
| `release` | required | Frees the slot, assigns it to the oldest waiting vehicle |
| `search` | required | Reports slot number, waiting status or not found |
| `undo` | not needed | Restores the previous snapshot |
| `display` (or empty) | not needed | Shows current status |

## Example Session

| Step | Action | Message |
|---|---|---|
| 1 | book `KA01AB1234` | Vehicle booked in slot 1. |
| 2 | book `KA01AB1234` again | This vehicle already has an active request. |
| 3 | book 11 more vehicles | Slots 2 to 12 filled |
| 4 | book `MH12XY4521` | Parking is full. Vehicle added to waiting queue. |
| 5 | release `KA01AB1234` | Slot 1 released and assigned to waiting vehicle MH12XY4521. |
| 6 | search `TN07ZZ1001` | Vehicle is in the waiting queue. |
| 7 | undo | Previous state restored. |

## Main Functions

`initialize`, `load_state`, `save_state`, `load_history`, `save_history`, `push_history`, `undo_last_change`, `enqueue`, `dequeue`, `find_vehicle`, `vehicle_is_waiting`, `book_vehicle`, `release_vehicle`, `url_decode`, `get_field`, `normalize_vehicle`, `html_escape`, `show_page`, `main` (dispatcher).

## Front-End Prototype

The screenshots in the report come from a browser dashboard prototype (HTML, CSS, JavaScript) that simulates the same rules using `localStorage`. In the full design, every request goes to the C program instead.

## Limitations

- Fixed at 12 slots and a single local instance.
- No file locking, so heavy simultaneous use is not safe.
- Every action reloads the page (CGI request/response).
- No sensors or number-plate detection.
- The **reset** action and the status filter are designed but not yet implemented.

## Future Scope

- Reset with confirmation and a status filter
- Safe file updates (write to temp file, then rename) and file locking
- Authentication and roles
- Configurable slot count, vehicle types and duration-based charges
- A database, only if the scope requires it

## Team

| Name | USN |
|---|---|
| Vijay R Sortur | NB25ISE266 |
| Virupakshi J | NB25ISE272 |
| Vineeth N | NB25ISE270 |
| Shashank Umesh Nelogal | NB25ISE209 |

**Faculty In-charge:** Mrs. Lakshmi M, Dept. of ISE, NMIT

## References

1. Kernighan & Ritchie, *The C Programming Language*, 2nd ed.
2. Cormen et al., *Introduction to Algorithms*, 4th ed.
3. RFC 3875, *The Common Gateway Interface (CGI) Version 1.1*
4. GNU GCC, https://gcc.gnu.org
