# CineBook 2.0 - Movie Ticket Booking System

A movie ticket booking desktop application built in C++ with a Win32 GUI and an SQLite database.

## Features
- Browse and search movies by title or genre
- Select a show date, show time and seat from a seat map
- Double-booking protection using SQLite transactions and a UNIQUE constraint
- Booking history and ticket cancellation
- Admin panel: add, update and delete movies, view the dashboard (movies, bookings, revenue)
- Console version also included (`src/main.cpp`)

## Tech Stack
- C++17
- Win32 API (GDI, owner-drawn controls)
- SQLite3

## Project Structure
```
src/gui.cpp        - GUI version (CineBook 2.0)
src/main.cpp       - Console version
database/cinebook.db - SQLite database
```

## Build (GUI version)
```
g++ src/gui.cpp -o CineBook.exe -std=c++17 -mwindows -lsqlite3 -lcomctl32
```

## Run
Set the admin password (12+ characters) before launching, then run from the project folder:
```
set CINEBOOK_ADMIN_PASSWORD=your-private-password
CineBook.exe
```

## Note
Do not compile `main.cpp` together with `gui.cpp`. They are separate entry points.
