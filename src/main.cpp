
#include <iostream>
#include <string>
#include <iomanip>
#include <limits>
#include <sqlite3.h>
using namespace std;

sqlite3* db = nullptr;

void connectDB() {
    if (sqlite3_open("database/cinebook.db", &db) != SQLITE_OK) {
        cout << "Database connection failed!\n";
        exit(1);
    }

    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS movies("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL, genre TEXT NOT NULL,"
        "price REAL NOT NULL, duration INTEGER NOT NULL);",
        nullptr, nullptr, nullptr);

    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS bookings("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "customer_name TEXT NOT NULL,"
        "movie_id INTEGER NOT NULL,"
        "show_time TEXT NOT NULL,"
        "seat_no TEXT NOT NULL,"
        "amount REAL NOT NULL,"
        "FOREIGN KEY(movie_id) REFERENCES movies(id),"
        "UNIQUE(movie_id,show_time,seat_no));",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM movies;", -1, &s, nullptr);
    sqlite3_step(s);
    int n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);

    if (n == 0) {
        sqlite3_exec(db,
            "INSERT INTO movies(name,genre,price,duration) VALUES"
            "('Interstellar','Sci-Fi',250,169),"
            "('Inception','Thriller',200,148),"
            "('Avengers','Action',300,180),"
            "('The Batman','Action',220,176);",
            nullptr, nullptr, nullptr);
    }
}

void viewMovies() {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT * FROM movies ORDER BY id;", -1, &s, nullptr);

    cout << "\n========== MOVIES ==========\n";

    while (sqlite3_step(s) == SQLITE_ROW) {
        cout << "\nID: " << sqlite3_column_int(s,0)
             << "\nName: " << sqlite3_column_text(s,1)
             << "\nGenre: " << sqlite3_column_text(s,2)
             << "\nPrice: Rs." << sqlite3_column_double(s,3)
             << "\nDuration: " << sqlite3_column_int(s,4) << " min\n";
    }
    sqlite3_finalize(s);
}

bool getMovie(int id, string& name, double& price) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT name,price FROM movies WHERE id=?;", -1, &s, nullptr);
    sqlite3_bind_int(s,1,id);

    bool found = false;
    if (sqlite3_step(s) == SQLITE_ROW) {
        name = (const char*)sqlite3_column_text(s,0);
        price = sqlite3_column_double(s,1);
        found = true;
    }
    sqlite3_finalize(s);
    return found;
}

void searchMovie() {
    string name;
    cin.ignore(numeric_limits<streamsize>::max(),'\n');
    cout << "Enter movie name: ";
    getline(cin,name);

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT * FROM movies WHERE name LIKE ?;", -1, &s, nullptr);

    string key = "%" + name + "%";
    sqlite3_bind_text(s,1,key.c_str(),-1,SQLITE_TRANSIENT);

    bool found = false;
    while (sqlite3_step(s) == SQLITE_ROW) {
        found = true;
        cout << "\nID: " << sqlite3_column_int(s,0)
             << "\nMovie: " << sqlite3_column_text(s,1)
             << "\nGenre: " << sqlite3_column_text(s,2)
             << "\nPrice: Rs." << sqlite3_column_double(s,3) << "\n";
    }
    if (!found) cout << "Movie not found!\n";
    sqlite3_finalize(s);
}

void addMovie() {
    string name,genre;
    double price;
    int duration;

    cin.ignore(numeric_limits<streamsize>::max(),'\n');
    cout << "Movie name: ";
    getline(cin,name);
    cout << "Genre: ";
    getline(cin,genre);
    cout << "Ticket price: ";
    cin >> price;
    cout << "Duration in minutes: ";
    cin >> duration;

    if (name.empty() || genre.empty() || price <= 0 || duration <= 0) {
        cout << "Invalid movie details!\n";
        return;
    }

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT INTO movies(name,genre,price,duration) VALUES(?,?,?,?);",
        -1,&s,nullptr);

    sqlite3_bind_text(s,1,name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,genre.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(s,3,price);
    sqlite3_bind_int(s,4,duration);

    if (sqlite3_step(s) == SQLITE_DONE)
        cout << "Movie added successfully!\n";
    else
        cout << "Could not add movie!\n";

    sqlite3_finalize(s);
}

void deleteMovie() {
    int id;
    cout << "Enter Movie ID: ";
    cin >> id;

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "DELETE FROM movies WHERE id=?;",-1,&s,nullptr);
    sqlite3_bind_int(s,1,id);

    if (sqlite3_step(s) == SQLITE_DONE && sqlite3_changes(db)>0)
        cout << "Movie deleted successfully!\n";
    else
        cout << "Movie not found or has existing bookings!\n";

    sqlite3_finalize(s);
}

// ---------- ADMIN DASHBOARD ----------
void adminDashboard() {
    sqlite3_stmt* s;

    cout << "\n====================================\n";
    cout << "          CINEBOOK DASHBOARD\n";
    cout << "====================================\n";

    sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM movies;",-1,&s,nullptr);
    sqlite3_step(s);
    cout << "\nTotal Movies: " << sqlite3_column_int(s,0);
    sqlite3_finalize(s);

    sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM bookings;",-1,&s,nullptr);
    sqlite3_step(s);
    cout << "\nTotal Bookings: " << sqlite3_column_int(s,0);
    sqlite3_finalize(s);

    sqlite3_prepare_v2(db,
        "SELECT COALESCE(SUM(amount),0) FROM bookings;",-1,&s,nullptr);
    sqlite3_step(s);
    cout << "\nTotal Revenue: Rs."
         << fixed << setprecision(2) << sqlite3_column_double(s,0);
    sqlite3_finalize(s);

    cout << "\n\nMost Booked Movie:\n";

    sqlite3_prepare_v2(db,
        "SELECT m.name,COUNT(b.id) AS total "
        "FROM movies m LEFT JOIN bookings b ON m.id=b.movie_id "
        "GROUP BY m.id ORDER BY total DESC,m.id ASC LIMIT 1;",
        -1,&s,nullptr);

    if (sqlite3_step(s) == SQLITE_ROW) {
        cout << sqlite3_column_text(s,0)
             << " (" << sqlite3_column_int(s,1) << " bookings)\n";
    }
    sqlite3_finalize(s);

    cout << "\n========== END DASHBOARD ==========\n";
}

bool adminLogin() {
    string password;
    cout << "Enter Admin Password: ";
    cin >> password;

    if (password == "admin123") {
        cout << "Admin login successful!\n";
        return true;
    }
    cout << "Incorrect password!\n";
    return false;
}

void adminPanel() {
    if (!adminLogin()) return;

    int choice;
    do {
        cout << "\n========== ADMIN PANEL ==========\n";
        cout << "1. Add Movie\n";
        cout << "2. Delete Movie\n";
        cout << "3. View Movies\n";
        cout << "4. Dashboard\n";
        cout << "5. Logout\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch(choice) {
            case 1: addMovie(); break;
            case 2: viewMovies(); deleteMovie(); break;
            case 3: viewMovies(); break;
            case 4: adminDashboard(); break;
            case 5: cout << "Logged out!\n"; break;
            default: cout << "Invalid choice!\n";
        }
    } while(choice != 5);
}

string chooseShow() {
    int choice;
    cout << "\n1. 10:00 AM\n2. 1:30 PM\n3. 5:00 PM\n4. 9:00 PM\n";
    cout << "Select show: ";
    cin >> choice;

    switch(choice) {
        case 1: return "10:00 AM";
        case 2: return "1:30 PM";
        case 3: return "5:00 PM";
        case 4: return "9:00 PM";
        default: return "";
    }
}

bool isBooked(int movieId,string show,string seat) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM bookings "
        "WHERE movie_id=? AND show_time=? AND seat_no=?;",
        -1,&s,nullptr);

    sqlite3_bind_int(s,1,movieId);
    sqlite3_bind_text(s,2,show.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,seat.c_str(),-1,SQLITE_TRANSIENT);

    sqlite3_step(s);
    bool booked = sqlite3_column_int(s,0)>0;
    sqlite3_finalize(s);
    return booked;
}

void displaySeats(int movieId,string show) {
    cout << "\n============= SCREEN =============\n";
    cout << "       1    2    3    4    5\n";

    for(char row='A';row<='D';row++) {
        cout << row << "   ";
        for(int col=1;col<=5;col++) {
            string seat = string(1,row)+to_string(col);
            cout << "[" << (isBooked(movieId,show,seat)?"X":"O") << "]  ";
        }
        cout << "\n";
    }
    cout << "O = Available | X = Booked\n";
}

void bookTicket() {
    viewMovies();

    int movieId;
    cout << "\nEnter Movie ID: ";
    cin >> movieId;

    string movie;
    double price;

    if(!getMovie(movieId,movie,price)) {
        cout << "Invalid Movie ID!\n";
        return;
    }

    string show = chooseShow();
    if(show.empty()) {
        cout << "Invalid show!\n";
        return;
    }

    displaySeats(movieId,show);

    string seat,customer;
    cout << "Enter seat (A1-D5): ";
    cin >> seat;

    if(seat.size()!=2 || seat[0]<'A' || seat[0]>'D' ||
       seat[1]<'1' || seat[1]>'5') {
        cout << "Invalid seat!\n";
        return;
    }

    if(isBooked(movieId,show,seat)) {
        cout << "Seat already booked!\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(),'\n');
    cout << "Customer name: ";
    getline(cin,customer);

    if(customer.empty()) {
        cout << "Customer name required!\n";
        return;
    }

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT INTO bookings(customer_name,movie_id,show_time,seat_no,amount) "
        "VALUES(?,?,?,?,?);",-1,&s,nullptr);

    sqlite3_bind_text(s,1,customer.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,2,movieId);
    sqlite3_bind_text(s,3,show.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,seat.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(s,5,price);

    if(sqlite3_step(s)==SQLITE_DONE) {
        cout << "\n========== TICKET CONFIRMED ==========\n";
        cout << "Booking ID: " << sqlite3_last_insert_rowid(db);
        cout << "\nCustomer: " << customer;
        cout << "\nMovie: " << movie;
        cout << "\nShow: " << show;
        cout << "\nSeat: " << seat;
        cout << "\nAmount: Rs." << fixed << setprecision(2) << price;
        cout << "\n======================================\n";
    } else {
        cout << "Booking failed! Try another seat.\n";
    }
    sqlite3_finalize(s);
}

void viewBookings() {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT b.id,b.customer_name,m.name,b.show_time,b.seat_no,b.amount "
        "FROM bookings b JOIN movies m ON b.movie_id=m.id "
        "ORDER BY b.id DESC;",-1,&s,nullptr);

    cout << "\n========== BOOKING HISTORY ==========\n";
    bool found=false;

    while(sqlite3_step(s)==SQLITE_ROW) {
        found=true;
        cout << "\nBooking ID: " << sqlite3_column_int(s,0);
        cout << "\nCustomer: " << sqlite3_column_text(s,1);
        cout << "\nMovie: " << sqlite3_column_text(s,2);
        cout << "\nShow: " << sqlite3_column_text(s,3);
        cout << "\nSeat: " << sqlite3_column_text(s,4);
        cout << "\nAmount: Rs." << sqlite3_column_double(s,5);
        cout << "\n----------------------------------\n";
    }

    if(!found) cout << "No bookings found!\n";
    sqlite3_finalize(s);
}

void cancelTicket() {
    int id;
    cout << "Enter Booking ID: ";
    cin >> id;

    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "DELETE FROM bookings WHERE id=?;",-1,&s,nullptr);
    sqlite3_bind_int(s,1,id);

    if(sqlite3_step(s)==SQLITE_DONE && sqlite3_changes(db)>0)
        cout << "Ticket cancelled successfully!\n";
    else
        cout << "Booking ID not found!\n";

    sqlite3_finalize(s);
}

int main() {
    connectDB();
    int choice;

    do {
        cout << "\n====================================\n";
        cout << "             CINEBOOK\n";
        cout << "      Movie Ticket Booking System\n";
        cout << "====================================\n";
        cout << "1. View Movies\n";
        cout << "2. Search Movie\n";
        cout << "3. Admin Panel\n";
        cout << "4. Book Ticket\n";
        cout << "5. View All Bookings\n";
        cout << "6. Cancel Ticket\n";
        cout << "7. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch(choice) {
            case 1: viewMovies(); break;
            case 2: searchMovie(); break;
            case 3: adminPanel(); break;
            case 4: bookTicket(); break;
            case 5: viewBookings(); break;
            case 6: cancelTicket(); break;
            case 7: cout << "Thank you for using CineBook!\n"; break;
            default: cout << "Invalid choice!\n";
        }

    } while(choice!=7);

    sqlite3_close(db);
    return 0;
}
