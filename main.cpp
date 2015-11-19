#include <iostream>
#include <fstream>
#include <sys/epoll.h>
#include <string>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

const string LOGIN_PASS_FILE_NAME = "/home/taqtaq11/ClionProjects/SSH_server/login_pass.txt";
const int MAXEVENTS = 64;
const int STANDART_PORT = 8080;

bool checkLoginAndPass(string inputLoginAndPass) ;
int createSocket(int port);
void makeNonBlocking(int ld);
void proceedRequest(int sd);
string executeCommand(string cmd);

int main() {
    int sfd, s;
    int efd;
    epoll_event event;
    epoll_event *events;

    sfd = createSocket(STANDART_PORT);
    s = listen(sfd, SOMAXCONN);

    efd = epoll_create1(0);

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);

    events = (epoll_event*)calloc(MAXEVENTS, sizeof(event));

    cout << "Server is ready" << endl;

    while (1) {
        int n;
        sockaddr in_addr;
        socklen_t in_len;

        n = epoll_wait(efd, events, MAXEVENTS, -1);
        cout << "New epoll event" << endl;
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
                    || (!(events[i].events & EPOLLIN))) {
                cerr << "epoll error" << endl;
                close(events[i].data.fd);
            }
            else if (sfd == events[i].data.fd) {
                while (1) {
                    in_len = sizeof(&in_addr);
                    int cfd = accept(sfd, &in_addr, &in_len);
                    if (cfd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        }
                        else {
                            cerr << "Socket accept error: " << errno << endl;
                            break;
                        }
                    }

                    makeNonBlocking(cfd);
                    event.data.fd = cfd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event);

                    cout << "New socket connection is observing" << endl;
                }
            }
            else {
                int chid = fork();

                if (chid == 0) {
                    proceedRequest(events[i].data.fd);
                    return 0;
                }
            }
        }
    }
}

bool checkLoginAndPass(string inputLoginAndPass) {
    ifstream loginAndPassFile (LOGIN_PASS_FILE_NAME);
    string line;

    if (loginAndPassFile.is_open()) {
        while (getline(loginAndPassFile, line)) {
            line.append("\n");
            if (line.compare(inputLoginAndPass) == 0) {
                loginAndPassFile.close();
                return true;
            }
        }
    }
    else {
        cerr << "Unable to open login-pass file" << endl;
    }

    loginAndPassFile.close();
    return false;
}

int createSocket(int port) {
    int ld = 0;
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    ld = socket(AF_INET, SOCK_STREAM, 0);
    if (ld == -1) {
        cerr << "Socket creation error" << endl;
    }

    if(bind(ld, (sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        cerr << "Binding error" << endl;
        exit(1);
    }

    makeNonBlocking(ld);

    return ld;
}

void makeNonBlocking(int ld) {
    int flags, s;

    flags = fcntl(ld, F_GETFL, 0);
    if (flags == -1) {
        cerr << "fcntl error" << endl;
    }

    flags |= O_NONBLOCK;
    s = fcntl(ld, F_SETFL, flags);
    if (flags == -1) {
        cerr << "fcntl error" << endl;
    }
}

void proceedRequest(int sd) {
    FILE* fd = fdopen(sd, "r");
    char* line;
    size_t len;
    int strNum = 0;

    while (getline(&line, &len, fd) != -1) {
        if (strNum == 0) {
            if (!checkLoginAndPass(line)) {
                string ln = "Unavailable login or pass";
                ssize_t sres = send(sd, ln.c_str(), ln.length(), 0);
                cout << "Login declined" << endl;
                break;
            }

            cout << "Login accepted" << endl;
        }
        else {
            string res = executeCommand(line);
            cout << "Command " + string(line) + " executed" << endl;
            ssize_t sres = send(sd, res.c_str(), res.length(), 0);
        }

        strNum++;
    }

    fclose(fd);
    close(sd);
}

string executeCommand(string cmd) {
    if (cmd.compare("wait") == 0) {
        usleep(15000000);
        return "Waited!!!";
    }

    FILE* f = popen(cmd.c_str(), "r");

    if (f == NULL) {
        cerr << "Popen error" << endl;
    }

    char ln[1000];
    string resLine;

    while (fgets(ln, sizeof(ln), f)) {
        cout << ln << endl;
        resLine.append(ln);
        resLine.append("\n");
    }

    pclose(f);
    return resLine;
}