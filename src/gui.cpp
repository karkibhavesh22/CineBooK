// CineBook 2.0 - Premium Dark Windows GUI
// Build from the project ROOT folder in VS Code terminal:
// g++ src/gui.cpp -o CineBook.exe -std=c++17 -mwindows -lsqlite3 -lcomctl32
//
// Run: .\CineBook.exe
// Uses the existing database/cinebook.db (or assets/database/cinebook.db).
// Set CINEBOOK_ADMIN_PASSWORD to a private 12+ character value before launch.
// This is a standalone GUI entry point: do NOT compile the old main.cpp with it.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>

static sqlite3* g_db = nullptr;
static HWND g_hwnd = nullptr;
static HFONT g_font = nullptr, g_titleFont = nullptr;
static std::vector<HWND> g_page;
struct ControlLayout { HWND window; RECT bounds; };
static std::vector<ControlLayout> g_layout;
static int g_selectedMovie = 0;
static int g_activeNav = 100;
static std::string g_selectedSeat;
static bool g_admin = false;
static HWND g_bookingSummary = nullptr;
static const COLORREF C_BG = RGB(12, 16, 27);
static const COLORREF C_PANEL = RGB(24, 31, 47);
static const COLORREF C_PANEL2 = RGB(32, 42, 61);
static const COLORREF C_BLUE = RGB(67, 126, 255);
static const COLORREF C_CYAN = RGB(49, 211, 220);
static const COLORREF C_WHITE = RGB(240, 244, 252);
static const COLORREF C_MUTED = RGB(157, 171, 194);
static const COLORREF C_GREEN = RGB(40, 190, 125);
static const COLORREF C_RED = RGB(220, 76, 91);
static const COLORREF C_GOLD = RGB(242, 183, 74);

struct Movie { int id; std::wstring name, genre; double price; int duration; };
static std::vector<Movie> g_movies;

static std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n ? n : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    if (!s.empty()) s.pop_back();
    return s;
}
static int clientWidth() {
    RECT r{};
    GetClientRect(g_hwnd, &r);
    return (std::max)(1L, r.right - r.left);
}
static int clientHeight() {
    RECT r{};
    GetClientRect(g_hwnd, &r);
    return (std::max)(1L, r.bottom - r.top);
}
static int sx(int x) {
    return MulDiv(x, clientWidth(), 1000);
}
static int sy(int y) {
    return MulDiv(y, clientHeight(), 740);
}
static std::wstring wide(const char* s) {
    if (!s) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n ? n : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    if (!w.empty()) w.pop_back();
    return w;
}
static std::wstring money(double v) {
    std::wostringstream o; o << L"\x20B9" << std::fixed << std::setprecision(2) << v; return o.str();
}
static void say(const std::wstring& s, const wchar_t* title=L"CineBook 2.0") {
    MessageBoxW(g_hwnd, s.c_str(), title, MB_OK | MB_ICONINFORMATION);
}
static void addPage(HWND h) { if (h) g_page.push_back(h); }
static void clearPage() {
    for (HWND h : g_page) if (IsWindow(h)) DestroyWindow(h);
    g_page.clear();
    g_layout.clear();
}
static HWND makeControl(const wchar_t* cls, const wchar_t* text, DWORD style,
                        int x, int y, int w, int h, int id=0) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD|WS_VISIBLE|style,
        sx(x),sy(y),sx(w),sy(h),g_hwnd,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    if (c) {
        SendMessageW(c,WM_SETFONT,(WPARAM)g_font,TRUE);
        addPage(c);
        g_layout.push_back({c,RECT{x,y,x+w,y+h}});
    }
    return c;
}
static HWND txt(const wchar_t* s,int x,int y,int w,int h,bool title=false) {
    HWND c=makeControl(L"STATIC",s,SS_LEFT,x,y,w,h);
    if(title) SendMessageW(c,WM_SETFONT,(WPARAM)g_titleFont,TRUE);
    return c;
}
static HWND btn(const wchar_t* s,int id,int x,int y,int w=140,int h=38) {
    return makeControl(L"BUTTON",s,BS_OWNERDRAW|BS_NOTIFY|WS_TABSTOP,x,y,w,h,id);
}
static HWND editBox(const wchar_t* hint,int id,int x,int y,int w=240) {
    HWND edit=makeControl(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,x,y,w,32,id);
    if(edit)SendMessageW(edit,EM_SETCUEBANNER,TRUE,(LPARAM)hint);
    return edit;
}
static HWND listBox(int id,int x,int y,int w,int h) {
    return makeControl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY|WS_TABSTOP,x,y,w,h,id);
}
static std::wstring value(int id) {
    HWND h=GetDlgItem(g_hwnd,id); int n=GetWindowTextLengthW(h);
    std::wstring s(n+1,L'\0'); GetWindowTextW(h,&s[0],n+1); s.resize(n); return s;
}
static int intValue(int id) {
    try { return std::stoi(utf8(value(id))); } catch (...) { return 0; }
}
static bool sql(const char* q) {
    char* err=nullptr; int rc=sqlite3_exec(g_db,q,nullptr,nullptr,&err);
    if(rc!=SQLITE_OK) {
        std::string m=err?err:"Database error";
        say(L"Database error:\n"+wide(m.c_str()),L"SQLite");
        sqlite3_free(err); return false;
    }
    return true;
}
static std::wstring trim(const std::wstring& text) {
    size_t first=text.find_first_not_of(L" \t\r\n");
    if(first==std::wstring::npos)return L"";
    size_t last=text.find_last_not_of(L" \t\r\n");
    return text.substr(first,last-first+1);
}
static bool initDB() {
    std::vector<std::wstring> paths;
    wchar_t executable[MAX_PATH]{};
    DWORD exeLength=GetModuleFileNameW(nullptr,executable,MAX_PATH);
    if(exeLength>0&&exeLength<MAX_PATH) {
        std::wstring directory(executable,exeLength);
        size_t slash=directory.find_last_of(L"\\/");
        if(slash!=std::wstring::npos) {
            directory.resize(slash+1);
            paths.push_back(directory+L"database\\cinebook.db");
            paths.push_back(directory+L"assets\\database\\cinebook.db");
        }
    }
    paths.push_back(L"database\\cinebook.db");
    paths.push_back(L"assets\\database\\cinebook.db");
    int rc=SQLITE_CANTOPEN;
    for(const std::wstring& path:paths) {
        DWORD attrs=GetFileAttributesW(path.c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::string utf8Path=utf8(path);
        rc=sqlite3_open_v2(utf8Path.c_str(),&g_db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,nullptr);
        if(rc==SQLITE_OK) break;
        if(g_db) sqlite3_close(g_db);
        g_db=nullptr;
    }
    if(rc!=SQLITE_OK) {
        say(L"Could not open an existing database\\cinebook.db or assets\\database\\cinebook.db. "
            L"Run CineBook from the project folder.",L"Database error");
        return false;
    }
    sqlite3_busy_timeout(g_db,5000);
    sqlite3_exec(g_db,"PRAGMA foreign_keys=ON;",nullptr,nullptr,nullptr);
    const char* required[]={
      "SELECT id,name,genre,price,duration FROM movies LIMIT 0;",
      "SELECT id,customer_name,movie_id,show_time,seat_no,amount FROM bookings LIMIT 0;"
    };
    for(const char* query:required) {
        sqlite3_stmt* st=nullptr;
        rc=sqlite3_prepare_v2(g_db,query,-1,&st,nullptr);
        if(st) sqlite3_finalize(st);
        if(rc!=SQLITE_OK) {
            std::string message="The existing database schema is incompatible: ";
            message+=sqlite3_errmsg(g_db);
            say(L"Database schema error:\n"+wide(message.c_str()),L"Database error");
            return false;
        }
    }
    if(!sql("CREATE UNIQUE INDEX IF NOT EXISTS idx_cinebook_seat_unique "
            "ON bookings(movie_id,show_time,seat_no);")) return false;
    return true;
}
static void loadMovies(const std::wstring& filter=L"") {
    g_movies.clear(); sqlite3_stmt* st=nullptr;
    const char* q=filter.empty()
      ?"SELECT id,name,genre,price,duration FROM movies ORDER BY id;"
      :"SELECT id,name,genre,price,duration FROM movies WHERE name LIKE ? OR genre LIKE ? ORDER BY id;";
    int rc=sqlite3_prepare_v2(g_db,q,-1,&st,nullptr);
    if(rc!=SQLITE_OK) {say(L"Could not load movies:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");return;}
    std::string f="%"+utf8(filter)+"%";
    if(!filter.empty()){sqlite3_bind_text(st,1,f.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,f.c_str(),-1,SQLITE_TRANSIENT);}
    while((rc=sqlite3_step(st))==SQLITE_ROW) {
        Movie m; m.id=sqlite3_column_int(st,0);
        m.name=wide((const char*)sqlite3_column_text(st,1));
        m.genre=wide((const char*)sqlite3_column_text(st,2));
        m.price=sqlite3_column_double(st,3); m.duration=sqlite3_column_int(st,4);
        g_movies.push_back(m);
    }
    sqlite3_finalize(st);
    if(rc!=SQLITE_DONE)say(L"Could not read movies:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");
}
static bool getMovie(int id,Movie& m) {
    sqlite3_stmt* st=nullptr;
    if(sqlite3_prepare_v2(g_db,"SELECT id,name,genre,price,duration FROM movies WHERE id=?;",-1,&st,nullptr)!=SQLITE_OK) {
        say(L"Could not read movie details:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");return false;}
    sqlite3_bind_int(st,1,id); bool ok=false;
    if(sqlite3_step(st)==SQLITE_ROW) {
        m.id=sqlite3_column_int(st,0);m.name=wide((const char*)sqlite3_column_text(st,1));
        m.genre=wide((const char*)sqlite3_column_text(st,2));m.price=sqlite3_column_double(st,3);
        m.duration=sqlite3_column_int(st,4);ok=true;
    }
    sqlite3_finalize(st); return ok;
}
static bool booked(int movie,const std::wstring& show,const std::string& seat) {
    sqlite3_stmt* st=nullptr;
    if(sqlite3_prepare_v2(g_db,"SELECT COUNT(*) FROM bookings WHERE movie_id=? AND seat_no=? "
        "AND (show_time=? OR show_time=?);",-1,&st,nullptr)!=SQLITE_OK) {
        say(L"Could not check seat availability:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");return false;}
    std::wstring legacy=show;
    size_t delimiter=show.find(L" | ");
    if(delimiter!=std::wstring::npos)legacy=show.substr(delimiter+3);
    std::string t=utf8(show),old=utf8(legacy);sqlite3_bind_int(st,1,movie);
    sqlite3_bind_text(st,2,seat.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,3,t.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,4,old.c_str(),-1,SQLITE_TRANSIENT);
    int rc=sqlite3_step(st);
    bool yes=rc==SQLITE_ROW&&sqlite3_column_int(st,0)>0;
    if(rc!=SQLITE_ROW)say(L"Could not check seat availability:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");
    sqlite3_finalize(st);return yes;
}
static std::wstring selectedShow() {
    HWND c=GetDlgItem(g_hwnd,5002);int i=(int)SendMessageW(c,CB_GETCURSEL,0,0);
    if(i<0)return L"";
    wchar_t b[80]{};SendMessageW(c,CB_GETLBTEXT,i,(LPARAM)b);return b;
}
static std::wstring todayDate() {
    std::time_t now=std::time(nullptr);
    std::tm local{};
    localtime_s(&local,&now);
    wchar_t date[16]{};
    swprintf_s(date,L"%04d-%02d-%02d",local.tm_year+1900,local.tm_mon+1,local.tm_mday);
    return date;
}
static bool validDate(const std::wstring& date) {
    if(date.size()!=10 || date[4]!=L'-' || date[7]!=L'-') return false;
    for(size_t i=0;i<date.size();++i)
        if(i!=4 && i!=7 && (date[i]<L'0' || date[i]>L'9')) return false;
    int year=std::stoi(date.substr(0,4));
    int month=std::stoi(date.substr(5,2));
    int day=std::stoi(date.substr(8,2));
    std::tm value{};
    value.tm_year=year-1900;value.tm_mon=month-1;value.tm_mday=day;
    std::time_t parsed=std::mktime(&value);
    if(parsed==-1) return false;
    std::tm normalized{};
    localtime_s(&normalized,&parsed);
    return normalized.tm_year==year-1900 && normalized.tm_mon==month-1 &&
           normalized.tm_mday==day;
}
static std::wstring bookingShow() {
    std::wstring date=trim(value(5004)),time=selectedShow();
    return validDate(date)&&!time.empty()?date+L" | "+time:L"";
}
static void fillShowCombo(int id) {
    HWND c=GetDlgItem(g_hwnd,id);
    const wchar_t* a[]={L"10:00 AM",L"1:30 PM",L"5:00 PM",L"9:00 PM"};
    for(auto s:a)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)s);
    SendMessageW(c,CB_SETCURSEL,0,0);
}
static void updateSeatButtons();
static void showHome();
static void showMovies(const std::wstring& filter=L"");
static void showBooking();
static void showHistory();
static void showAdmin();
static void showAdminBookings();

static void nav(const wchar_t* label,int id,int x) {
    btn(label,id,x,76,128,36);
}
static void shell() {
    clearPage();
    txt(L"CINEBOOK",30,20,250,42,true);
    txt(L"THE MOVIE EXPERIENCE",245,30,340,24);
    nav(L"HOME",100,30);nav(L"MOVIES",101,166);nav(L"SEARCH",102,302);
    nav(L"BOOK TICKETS",103,438);nav(L"MY BOOKINGS",104,574);nav(L"ADMIN",105,710);
    txt(L"____________________________________________________________________________________________________________",30,119,930,18);
}
static void movieCard(const Movie&,int x,int y,int buttonId,int) {
    btn(L"",buttonId,x,y,205,270);
}
static void showHome() {
    g_activeNav=100;shell();loadMovies();
    txt(L"YOUR NEXT MOVIE NIGHT STARTS HERE",38,155,800,45,true);
    txt(L"Discover stories worth watching. Pick your movie and reserve your favourite seat.",42,205,820,28);
    btn(L"EXPLORE MOVIES",201,42,252,180,44);
    btn(L"BOOK A TICKET",202,235,252,180,44);
    txt(L"NOW SHOWING",42,325,300,30,true);
    int x=42;for(size_t i=0;i<g_movies.size()&&i<4;i++,x+=222)movieCard(g_movies[i],x,365,2100+(int)i,(int)i);
    if(g_movies.empty())txt(L"No movies are available in the database yet.",42,375,520,28);
}
static void showMovies(const std::wstring& filter) {
    g_activeNav=101;shell();loadMovies(filter);
    txt(filter.empty()?L"EXPLORE ALL MOVIES":L"SEARCH RESULTS",40,150,600,42,true);
    txt(L"Choose a movie to view its booking options.",43,198,650,25);
    listBox(4004,42,235,880,380);
    HWND list=GetDlgItem(g_hwnd,4004);
    for(const Movie& m:g_movies) {
        std::wstring row=L"#"+std::to_wstring(m.id)+L"   "+m.name+L"   |   "+
            m.genre+L"   |   "+std::to_wstring(m.duration)+L" min   |   "+money(m.price);
        SendMessageW(list,LB_ADDSTRING,0,(LPARAM)row.c_str());
    }
    if(g_movies.empty())txt(L"No movies found. Try another search.",45,260,500,30);
    else btn(L"BOOK SELECTED MOVIE",4005,42,635,230,40);
}
static void showSearch() {
    g_activeNav=102;shell();txt(L"FIND YOUR MOVIE",40,155,500,42,true);
    txt(L"Search by title or genre",43,205,300,24);
    editBox(L"Type a movie name or genre...",4001,42,242,340);
    btn(L"SEARCH",4002,397,240,125,36);
    btn(L"SHOW ALL",4003,535,240,125,36);
    listBox(4004,42,300,820,320);
    loadMovies();
    for(auto&m:g_movies){std::wstring row=L"#"+std::to_wstring(m.id)+L"   "+m.name+L"   |   "+m.genre+L"   |   "+money(m.price);
      SendMessageW(GetDlgItem(g_hwnd,4004),LB_ADDSTRING,0,(LPARAM)row.c_str());}
}
static void showBooking() {
    g_activeNav=103;shell();loadMovies();g_selectedSeat.clear();g_bookingSummary=nullptr;
    txt(L"RESERVE YOUR SEAT",40,145,520,42,true);
    txt(L"1. Movie ID",43,198,120,24);editBox(L"Movie ID",5001,165,193,130);
    if(g_selectedMovie>0)SetWindowTextW(GetDlgItem(g_hwnd,5001),std::to_wstring(g_selectedMovie).c_str());
    txt(L"2. Show time",43,239,120,24);
    makeControl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,165,234,150,180,5002);fillShowCombo(5002);
    txt(L"3. Customer name",43,280,140,24);editBox(L"Full name",5003,165,275,240);
    txt(L"4. Show date",43,320,120,24);
    editBox(L"YYYY-MM-DD",5004,165,315,150);
    SetWindowTextW(GetDlgItem(g_hwnd,5004),todayDate().c_str());
    txt(L"5. Select a seat",43,350,250,28,true);
    txt(L"SCREEN",460,322,140,24);
    // Screen indicator
    btn(L"",5994,330,350,390,10);
    txt(L"AVAILABLE",760,365,100,20);txt(L"BOOKED",760,395,100,20);txt(L"SELECTED",760,425,100,20);
    btn(L"",5991,735,365,16,16);btn(L"",5992,735,395,16,16);btn(L"",5993,735,425,16,16);
    const wchar_t* rows[]={L"A",L"B",L"C",L"D"};
    for(int r=0;r<4;r++)for(int c=1;c<=5;c++){
        std::wstring seat=rows[r]+std::to_wstring(c);
        int id=5100+r*5+c;
        btn(seat.c_str(),id,340+(c-1)*68,382+r*53,52,39);
    }
    btn(L"CHECK SEAT MAP",5200,43,570,160,40);
    btn(L"CONFIRM BOOKING",5201,215,570,190,40);
    txt(L"Ticket price is taken directly from the movie database.",430,578,460,24);
    g_bookingSummary=txt(L"Choose a movie and seat to see the total.",430,608,490,28,true);
    updateSeatButtons();
}
static void updateSeatButtons() {
    int movie=intValue(5001);std::wstring show=bookingShow();
    for(int r=0;r<4;r++)for(int c=1;c<=5;c++){
        std::string seat(1,char('A'+r));seat+=char('0'+c);
        HWND h=GetDlgItem(g_hwnd,5100+r*5+c);if(!h)continue;
        bool isBooked=movie>0&&!show.empty()&&booked(movie,show,seat);
        SetWindowTextW(h,wide(seat.c_str()).c_str());
        InvalidateRect(h,nullptr,TRUE);
        // Store state in control ID; owner draw queries DB and selected seat.
        (void)isBooked;
    }
    if(g_bookingSummary) {
        Movie m{};
        std::wstring summary=L"Select a movie and available seat.";
        if(movie>0&&getMovie(movie,m))
            summary=L"Ticket "+money(m.price)+L"   |   Seat "+
                (g_selectedSeat.empty()?L"--":wide(g_selectedSeat.c_str()))+
                L"   |   Total "+money(m.price);
        SetWindowTextW(g_bookingSummary,summary.c_str());
    }
}
static void showReceipt(int bookingId,const std::wstring& customer,const Movie& m,
                        const std::wstring& show,const std::string& seat) {
    std::wostringstream r;
    r<<L"CINEBOOK  |  E-TICKET\n"
     <<L"----------------------------------\n"
     <<L"BOOKING CONFIRMED\n\n"
     <<L"Booking ID : #"<<bookingId<<L"\n"
     <<L"Name       : "<<customer<<L"\n"
     <<L"Movie      : "<<m.name<<L"\n"
     <<L"Genre      : "<<m.genre<<L"\n"
     <<L"Show       : "<<show<<L"\n"
     <<L"Seat       : "<<wide(seat.c_str())<<L"\n"
     <<L"Amount     : "<<money(m.price)<<L"\n"
     <<L"----------------------------------\n"
     <<L"Please arrive 15 minutes before showtime.\nEnjoy your movie!";
    MessageBoxW(g_hwnd,r.str().c_str(),L"CineBook - Your Ticket",MB_OK|MB_ICONINFORMATION);
}
static void bookTicket() {
    int id=intValue(5001);    std::wstring customer=trim(value(5003)),show=bookingShow();
    if(id<=0||customer.empty()||customer==L"Full name"||show.empty()){
        say(L"Enter a valid Movie ID, show date, show time and customer name.");return;}
    if(g_selectedSeat.empty()){say(L"Please select an available seat first.");return;}
    if(booked(id,show,g_selectedSeat)){say(L"This seat has already been booked. Choose another.");updateSeatButtons();return;}
    Movie m;if(!getMovie(id,m)){say(L"Movie ID not found.");return;}
    if(!sql("BEGIN IMMEDIATE;")) return;
    if(booked(id,show,g_selectedSeat)) {
        sql("ROLLBACK;");
        say(L"This seat has just been booked for this show. Choose another.");
        updateSeatButtons();
        return;
    }
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(g_db,"INSERT INTO bookings(customer_name,movie_id,show_time,seat_no,amount) VALUES(?,?,?,?,?);",-1,&st,nullptr);
    if(rc!=SQLITE_OK) {
        sql("ROLLBACK;");
        say(L"Could not prepare the booking. Check the database schema.",L"Database error");
        return;
    }
    std::string c=utf8(customer),t=utf8(show);
    sqlite3_bind_text(st,1,c.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(st,2,id);
    sqlite3_bind_text(st,3,t.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,4,g_selectedSeat.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(st,5,m.price);
    bool ok=sqlite3_step(st)==SQLITE_DONE;
    int bookingId=ok?(int)sqlite3_last_insert_rowid(g_db):0;
    if(!ok) {
        std::string message=sqlite3_errmsg(g_db);
        sqlite3_finalize(st);
        sql("ROLLBACK;");
        if(std::string(message).find("UNIQUE")!=std::string::npos)
            say(L"This seat has just been booked for this show. Choose another.");
        else
            say(L"Booking failed:\n"+wide(message.c_str()),L"Database error");
        updateSeatButtons();
        return;
    }
    sqlite3_finalize(st);
    ok=sql("COMMIT;");
    if(ok){showReceipt(bookingId,customer,m,show,g_selectedSeat);g_selectedSeat.clear();showBooking();}
    else {sql("ROLLBACK;");say(L"Booking could not be committed.",L"Database error");}
}
static void showHistory() {
    g_activeNav=104;shell();txt(L"MY BOOKINGS",40,155,500,42,true);
    listBox(6001,42,215,900,330);
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(g_db,"SELECT b.id,b.customer_name,m.name,b.show_time,b.seat_no,b.amount "
      "FROM bookings b JOIN movies m ON b.movie_id=m.id ORDER BY b.id DESC;",-1,&st,nullptr);
    if(rc!=SQLITE_OK) {say(L"Could not load booking history:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");return;}
    HWND l=GetDlgItem(g_hwnd,6001);
    bool found=false;
    while(sqlite3_step(st)==SQLITE_ROW){
        found=true;
        std::wostringstream o;o<<L"#"<<sqlite3_column_int(st,0)<<L"  |  "
          <<wide((const char*)sqlite3_column_text(st,1))<<L"  |  "
          <<wide((const char*)sqlite3_column_text(st,2))<<L"  |  "
          <<wide((const char*)sqlite3_column_text(st,3))<<L"  |  Seat "
          <<wide((const char*)sqlite3_column_text(st,4))<<L"  |  "
          <<money(sqlite3_column_double(st,5));
        std::wstring line=o.str();SendMessageW(l,LB_ADDSTRING,0,(LPARAM)line.c_str());
    }
    sqlite3_finalize(st);
    if(!found)txt(L"No bookings have been made yet.",43,555,480,25);
    txt(L"Cancel ticket using booking ID",42,570,260,25);editBox(L"Booking ID",6002,310,565,150);
    btn(L"CANCEL BOOKING",6003,475,563,170,38);
}
static void showAdmin() {
    g_activeNav=105;shell();
    if(!g_admin){
        txt(L"ADMIN ACCESS",40,160,500,42,true);
        txt(L"Sign in to manage movies and monitor CineBook performance.",43,210,700,25);
        editBox(L"Admin password",7001,43,255,250);
        SendMessageW(GetDlgItem(g_hwnd,7001),EM_SETPASSWORDCHAR,L'*',0);
        btn(L"SIGN IN",7002,310,252,120,36);
        txt(L"Set CINEBOOK_ADMIN_PASSWORD before launching the app.",43,310,520,22);return;
    }
    int mc=0,bc=0;double rev=0;sqlite3_stmt* st=nullptr;
    sqlite3_prepare_v2(g_db,"SELECT COUNT(*) FROM movies;",-1,&st,nullptr);if(sqlite3_step(st)==SQLITE_ROW)mc=sqlite3_column_int(st,0);sqlite3_finalize(st);
    sqlite3_prepare_v2(g_db,"SELECT COUNT(*),COALESCE(SUM(amount),0) FROM bookings;",-1,&st,nullptr);
    if(sqlite3_step(st)==SQLITE_ROW){bc=sqlite3_column_int(st,0);rev=sqlite3_column_double(st,1);}sqlite3_finalize(st);
    txt(L"ADMIN DASHBOARD",40,145,500,42,true);
    std::wstring stats=L"MOVIES  "+std::to_wstring(mc)+L"       BOOKINGS  "+std::to_wstring(bc)+L"       REVENUE  "+money(rev);
    txt(stats.c_str(),43,198,850,34,true);
    txt(L"ADD / EDIT MOVIE",43,255,300,28,true);
    txt(L"Select a movie from the list to edit it.",43,282,360,22);
    txt(L"Title",43,300,80,22);editBox(L"Movie title",7003,140,295,210);
    txt(L"Genre",43,340,80,22);editBox(L"Genre",7004,140,335,210);
    txt(L"Price",43,380,80,22);editBox(L"Ticket price",7005,140,375,210);
    txt(L"Minutes",43,420,80,22);editBox(L"Duration",7006,140,415,210);
    btn(L"ADD",7007,43,465,110,38);btn(L"UPDATE",7013,165,465,120,38);
    txt(L"DELETE MOVIE ID",43,530,145,22);editBox(L"Movie ID",7008,195,525,110);
    btn(L"DELETE",7009,320,523,105,36);
    btn(L"REFRESH",7010,43,585,125,36);btn(L"BOOKINGS",7014,185,585,135,36);
    btn(L"LOG OUT",7011,330,585,125,36);
    listBox(7012,475,255,450,340);loadMovies();
    HWND l=GetDlgItem(g_hwnd,7012);
    for(auto&m:g_movies){std::wstring line=L"#"+std::to_wstring(m.id)+L"  "+m.name+L"  |  "+m.genre+L"  |  "+money(m.price);
      SendMessageW(l,LB_ADDSTRING,0,(LPARAM)line.c_str());}
}
static void showAdminBookings() {
    g_activeNav=105;shell();
    if(!g_admin) {showAdmin();return;}
    txt(L"BOOKING MANAGEMENT",40,155,600,42,true);
    listBox(7020,42,215,900,325);
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(g_db,"SELECT b.id,b.customer_name,m.name,b.show_time,b.seat_no,b.amount "
        "FROM bookings b JOIN movies m ON m.id=b.movie_id ORDER BY b.id DESC;",-1,&st,nullptr);
    if(rc!=SQLITE_OK) {say(L"Could not load bookings:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");return;}
    HWND l=GetDlgItem(g_hwnd,7020);
    bool found=false;
    while(sqlite3_step(st)==SQLITE_ROW) {
        found=true;
        std::wostringstream row;
        row<<L"#"<<sqlite3_column_int(st,0)<<L" | "
           <<wide((const char*)sqlite3_column_text(st,1))<<L" | "
           <<wide((const char*)sqlite3_column_text(st,2))<<L" | "
           <<wide((const char*)sqlite3_column_text(st,3))<<L" | Seat "
           <<wide((const char*)sqlite3_column_text(st,4))<<L" | "
           <<money(sqlite3_column_double(st,5));
        std::wstring text=row.str();
        SendMessageW(l,LB_ADDSTRING,0,(LPARAM)text.c_str());
    }
    sqlite3_finalize(st);
    if(!found)txt(L"No customer bookings are available.",43,555,500,25);
    txt(L"Cancel booking ID",42,575,180,25);editBox(L"Booking ID",7021,220,570,140);
    btn(L"CANCEL",7022,378,568,120,38);btn(L"REFRESH",7023,515,568,130,38);
    btn(L"MOVIE DASHBOARD",7024,660,568,180,38);
}
static void drawButton(const DRAWITEMSTRUCT* d) {
    int id=(int)d->CtlID;RECT r=d->rcItem;HDC dc=d->hDC;
    COLORREF fill=C_BLUE;
    if(id>=100&&id<=105)fill=id==g_activeNav?RGB(31,157,191):RGB(32,43,63);
    bool card=(id>=2100&&id<2200)||(id>=2200&&id<2300);
    if(card)fill=C_PANEL;
    if(id>=5101&&id<=5120){
        int n=id-5101;int row=n/5,col=n%5;
        std::string seat(1,char('A'+row));seat+=char('1'+col);
        bool isBooked=intValue(5001)>0&&booked(intValue(5001),bookingShow(),seat);
        fill=isBooked?C_RED:(g_selectedSeat==seat?C_CYAN:C_GREEN);
    }
    if(id==5991)fill=C_GREEN;
    if(id==5992)fill=C_RED;
    if(id==5993||id==5994)fill=C_CYAN;
    if(id==201||id==202||id==5201||id==7002||id==7007||id==7013||id==7022)fill=C_BLUE;
    if(d->itemState&ODS_HOTLIGHT)fill=RGB(56,162,210);
    if(d->itemState&ODS_SELECTED)fill=RGB(45,91,178);
    HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,fill);
    HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,p);
    LONG radius=(std::min)(12L,(std::min)(r.right-r.left,r.bottom-r.top)/2);
    RoundRect(dc,r.left+1,r.top+1,r.right-1,r.bottom-1,radius,radius);
    SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);
    if(card) {
        int index=id>=2200?id-2200:id-2100;
        if(index>=0&&index<(int)g_movies.size()) {
            static const COLORREF accents[]={RGB(69,91,164),RGB(128,65,112),RGB(35,122,132),RGB(153,91,47)};
            RECT poster{r.left+sx(12),r.top+sy(38),r.right-sx(12),r.top+sy(166)};
            HBRUSH posterBrush=CreateSolidBrush(accents[index%4]);
            FillRect(dc,&poster,posterBrush);DeleteObject(posterBrush);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,C_WHITE);
            HFONT old=(HFONT)SelectObject(dc,g_titleFont);
            DrawTextW(dc,g_movies[(size_t)index].name.c_str(),-1,&poster,DT_CENTER|DT_VCENTER|DT_WORDBREAK);
            SelectObject(dc,old);
            RECT title{r.left+sx(13),r.top+sy(12),r.right-sx(8),r.top+sy(34)};
            std::wstring label=L"FILM  /  "+std::to_wstring(g_movies[(size_t)index].id);
            DrawTextW(dc,label.c_str(),-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            RECT info{r.left+sx(13),r.top+sy(176),r.right-sx(8),r.bottom-sy(9)};
            std::wstring details=g_movies[(size_t)index].name+L"\n"+
                g_movies[(size_t)index].genre+L"\n"+
                std::to_wstring(g_movies[(size_t)index].duration)+L" min   |   "+
                money(g_movies[(size_t)index].price);
            HFONT infoFont=(HFONT)SelectObject(dc,g_font);
            DrawTextW(dc,details.c_str(),-1,&info,DT_LEFT|DT_TOP|DT_WORDBREAK);
            SelectObject(dc,infoFont);
        }
        return;
    }
    wchar_t label[256]{};GetWindowTextW(d->hwndItem,label,255);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,(id>=5101&&id<=5120&&fill==C_CYAN)?RGB(10,24,35):C_WHITE);
    HFONT f=(HFONT)SendMessageW(d->hwndItem,WM_GETFONT,0,0);HGDIOBJ of=f?SelectObject(dc,f):nullptr;
    DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(of)SelectObject(dc,of);
}
static bool readMovieFields(std::string& name,std::string& genre,double& price,int& duration) {
    name=utf8(trim(value(7003)));genre=utf8(trim(value(7004)));
    std::string priceText=utf8(trim(value(7005))),durationText=utf8(trim(value(7006)));
    try {
        size_t priceUsed=0,durationUsed=0;
        price=std::stod(priceText,&priceUsed);
        duration=std::stoi(durationText,&durationUsed);
        if(priceUsed!=priceText.size()||durationUsed!=durationText.size())return false;
    } catch(...) {return false;}
    return !name.empty()&&name.size()<=200&&!genre.empty()&&genre.size()<=100&&
           std::isfinite(price)&&price>0&&price<=1000000&&duration>0&&duration<=1000;
}
static void command(int id) {
    if(id>=2100&&id<2100+(int)g_movies.size()){g_selectedMovie=g_movies[id-2100].id;showBooking();SetWindowTextW(GetDlgItem(g_hwnd,5001),std::to_wstring(g_selectedMovie).c_str());return;}
    if(id>=2200&&id<2200+(int)g_movies.size()){g_selectedMovie=g_movies[id-2200].id;showBooking();SetWindowTextW(GetDlgItem(g_hwnd,5001),std::to_wstring(g_selectedMovie).c_str());return;}
    if(id>=5101&&id<=5120){
        int n=id-5101;std::string seat(1,char('A'+n/5));seat+=char('1'+n%5);
        int movie=intValue(5001);auto show=bookingShow();
        if(movie<=0||show.empty()){say(L"Enter Movie ID and select show time before choosing a seat.");return;}
        if(booked(movie,show,seat)){say(L"That seat is already booked.");return;}
        g_selectedSeat=seat;updateSeatButtons();return;
    }
    switch(id){
      case 100:showHome();break;case 101:showMovies();break;case 102:showSearch();break;
      case 103:showBooking();break;case 104:showHistory();break;case 105:showAdmin();break;
      case 201:case 202:showMovies();break;
      case 4002:showMovies(value(4001));break;case 4003:showMovies();break;
      case 4005:{
        LRESULT selected=SendMessageW(GetDlgItem(g_hwnd,4004),LB_GETCURSEL,0,0);
        if(selected==LB_ERR||selected>=(LRESULT)g_movies.size()){say(L"Select a movie first.");break;}
        g_selectedMovie=g_movies[(size_t)selected].id;showBooking();break;}
      case 5200:updateSeatButtons();say(L"Seat map refreshed. Green = available, red = booked, cyan = selected.");break;
      case 5201:bookTicket();break;
      case 6003:{
        int bid=intValue(6002);if(bid<=0){say(L"Enter a valid booking ID.");break;}
        sqlite3_stmt* st=nullptr;sqlite3_prepare_v2(g_db,"DELETE FROM bookings WHERE id=?;",-1,&st,nullptr);
        sqlite3_bind_int(st,1,bid);bool ok=sqlite3_step(st)==SQLITE_DONE&&sqlite3_changes(g_db)>0;sqlite3_finalize(st);
        say(ok?L"Booking cancelled successfully.":L"Booking ID not found.");showHistory();break;}
      case 7002:{
        char password[512]{};
        DWORD length=GetEnvironmentVariableA("CINEBOOK_ADMIN_PASSWORD",password,(DWORD)sizeof(password));
        if(length==0) {say(L"Admin access is not configured. Set CINEBOOK_ADMIN_PASSWORD (at least 12 characters) before launching.");break;}
        if(length>=sizeof(password)) {say(L"The configured admin password is too long.",L"Configuration error");break;}
        if(length<12) {say(L"CINEBOOK_ADMIN_PASSWORD must contain at least 12 characters.",L"Configuration error");break;}
        if(utf8(value(7001))==password){g_admin=true;showAdmin();}
        else say(L"Incorrect admin password.");
        SecureZeroMemory(password,sizeof(password));
        break;}
      case 7007:{
        std::string name,genre;double price=0;int duration=0;
        if(!readMovieFields(name,genre,price,duration)){say(L"Enter a title (max 200 characters), genre (max 100), valid price and duration.");break;}
        sqlite3_stmt* st=nullptr;
        if(sqlite3_prepare_v2(g_db,"INSERT INTO movies(name,genre,price,duration) VALUES(?,?,?,?);",-1,&st,nullptr)!=SQLITE_OK){
            say(L"Could not prepare movie insert:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");break;}
        sqlite3_bind_text(st,1,name.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,genre.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_double(st,3,price);sqlite3_bind_int(st,4,duration);
        bool ok=sqlite3_step(st)==SQLITE_DONE;
        std::string error=sqlite3_errmsg(g_db);sqlite3_finalize(st);
        if(ok)say(L"Movie added successfully.");
        else say(L"Could not add movie:\n"+wide(error.c_str()),L"Database error");
        showAdmin();break;}
      case 7013:{
        int mid=intValue(7008);std::string name,genre;double price=0;int duration=0;
        if(mid<=0||!readMovieFields(name,genre,price,duration)){say(L"Select a movie and enter valid title, genre, price and duration.");break;}
        sqlite3_stmt* st=nullptr;
        if(sqlite3_prepare_v2(g_db,"UPDATE movies SET name=?,genre=?,price=?,duration=? WHERE id=?;",-1,&st,nullptr)!=SQLITE_OK){
            say(L"Could not prepare movie update:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");break;}
        sqlite3_bind_text(st,1,name.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,genre.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_double(st,3,price);sqlite3_bind_int(st,4,duration);sqlite3_bind_int(st,5,mid);
        bool ok=sqlite3_step(st)==SQLITE_DONE&&sqlite3_changes(g_db)>0;
        std::string error=sqlite3_errmsg(g_db);sqlite3_finalize(st);
        if(ok)say(L"Movie updated successfully.");
        else say(L"Could not update movie:\n"+wide(error.c_str()),L"Database error");
        showAdmin();break;}
      case 7009:{
        int mid=intValue(7008);if(mid<=0){say(L"Enter a valid movie ID.");break;}
        sqlite3_stmt* st=nullptr;sqlite3_prepare_v2(g_db,"DELETE FROM movies WHERE id=?;",-1,&st,nullptr);
        sqlite3_bind_int(st,1,mid);bool ok=sqlite3_step(st)==SQLITE_DONE&&sqlite3_changes(g_db)>0;sqlite3_finalize(st);
        say(ok?L"Movie deleted.":L"Movie not found or existing bookings prevent deletion.");showAdmin();break;}
      case 7010:showAdmin();break;case 7011:g_admin=false;showAdmin();break;
      case 7014:showAdminBookings();break;
      case 7022:case 7023:{
        if(id==7023){showAdminBookings();break;}
        int bid=intValue(7021);
        if(bid<=0){say(L"Enter a valid booking ID.");break;}
        if(MessageBoxW(g_hwnd,L"Cancel this booking and release its seat?",L"Confirm cancellation",MB_YESNO|MB_ICONWARNING)!=IDYES)break;
        sqlite3_stmt* st=nullptr;
        if(sqlite3_prepare_v2(g_db,"DELETE FROM bookings WHERE id=?;",-1,&st,nullptr)!=SQLITE_OK){
            say(L"Could not prepare cancellation:\n"+wide(sqlite3_errmsg(g_db)),L"Database error");break;}
        sqlite3_bind_int(st,1,bid);bool ok=sqlite3_step(st)==SQLITE_DONE&&sqlite3_changes(g_db)>0;
        std::string error=sqlite3_errmsg(g_db);sqlite3_finalize(st);
        if(ok)say(L"Booking cancelled.");
        else say(L"Booking cancellation failed:\n"+wide(error.c_str()),L"Database error");
        showAdminBookings();break;}
      case 7024:showAdmin();break;
    }
}
static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg){
      case WM_CREATE:
        g_hwnd=h;
        g_font=CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_titleFont=CreateFontW(27,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        showHome();return 0;
      case WM_COMMAND:{
        int id=LOWORD(wp),notification=HIWORD(wp);
        if(id==7012&&notification==LBN_SELCHANGE) {
            LRESULT index=SendMessageW(GetDlgItem(h,7012),LB_GETCURSEL,0,0);
            if(index!=LB_ERR&&index<(LRESULT)g_movies.size()) {
                const Movie& movie=g_movies[(size_t)index];
                SetWindowTextW(GetDlgItem(h,7008),std::to_wstring(movie.id).c_str());
                SetWindowTextW(GetDlgItem(h,7003),movie.name.c_str());
                SetWindowTextW(GetDlgItem(h,7004),movie.genre.c_str());
                SetWindowTextW(GetDlgItem(h,7005),std::to_wstring(movie.price).c_str());
                SetWindowTextW(GetDlgItem(h,7006),std::to_wstring(movie.duration).c_str());
            }
            return 0;
        }
        if(id==4004&&notification==LBN_DBLCLK){command(4005);return 0;}
        if((id==5001||id==5004)&&notification==EN_CHANGE){updateSeatButtons();return 0;}
        if(id==5002&&notification==CBN_SELCHANGE){updateSeatButtons();return 0;}
        if(notification==BN_CLICKED||notification==0)command(id);
        return 0;}
      case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
      case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;SetTextColor(dc,C_WHITE);SetBkColor(dc,C_BG);static HBRUSH b=CreateSolidBrush(C_BG);return(LRESULT)b;}
      case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,RGB(20,25,36));SetBkColor(dc,RGB(245,247,252));static HBRUSH b=CreateSolidBrush(RGB(245,247,252));return(LRESULT)b;}
      case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,C_WHITE);SetBkColor(dc,C_PANEL);static HBRUSH b=CreateSolidBrush(C_PANEL);return(LRESULT)b;}
      case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);HBRUSH b=CreateSolidBrush(C_BG);FillRect((HDC)wp,&r,b);DeleteObject(b);return 1;}
      case WM_SIZE:
        for(const ControlLayout& item:g_layout) {
            if(IsWindow(item.window))SetWindowPos(item.window,nullptr,
                sx(item.bounds.left),sy(item.bounds.top),
                sx(item.bounds.right-item.bounds.left),sy(item.bounds.bottom-item.bounds.top),
                SWP_NOZORDER|SWP_NOACTIVATE);
        }
        return 0;
      case WM_GETMINMAXINFO:{
        MINMAXINFO* limits=(MINMAXINFO*)lp;
        limits->ptMinTrackSize.x=800;limits->ptMinTrackSize.y=640;return 0;}
      case WM_DESTROY:
        if(g_db){sqlite3_close(g_db);g_db=nullptr;}
        if(g_font)DeleteObject(g_font);
        if(g_titleFont)DeleteObject(g_titleFont);
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE,LPSTR,int show) {
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES};InitCommonControlsEx(&icc);
    if(!initDB()){if(g_db)sqlite3_close(g_db);return 1;}
    WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"CineBook20Window";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);wc.style=CS_HREDRAW|CS_VREDRAW;
    RegisterClassW(&wc);
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"CineBook 2.0 | Premium Movie Booking",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1000,790,nullptr,nullptr,inst,nullptr);
    if(!w){sqlite3_close(g_db);return 1;}
    ShowWindow(w,show);UpdateWindow(w);
    MSG m;while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}
    return (int)m.wParam;
}
