
#include <string>
#include <vector>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
struct Server{
    std::string host;
    int port;
};
std::vector<Server> servers={
    {"127.0.0.1", 9001},
    {"127.0.0.1", 9002},
    {"127.0.0.1", 9003}
};

std::atomic<size_t> rr_counter{0};
/*return a reference to the stuct object in servers, 
so anychange will also reflect there
*/
const Server& pick_server(){
    size_t i = rr_counter.fetch_add(1)%servers.size();
    return servers[i];

}
/*return a file descriptor, also make sure we don't
change server by accident with const server input
 */
int connect_to_server(const Server& server){
    // this socket is for IPv4, and TCP
    int fd = socket(AF_INET, SOCK_STREAM,0);

    sockaddr_in addr{};
    //config kind of address =ipv4 so connect()) recognize the struct content
    addr.sin_family=AF_INET;
    //host to network convert
    addr.sin_port=htons(server.port);
    //convert ip int to raw byte hex
    inet_pton(AF_INET,server.host.c_str(),&addr.sin_addr);
    if (connect(fd,(sockaddr*)&addr,sizeof(addr))<0){
        close(fd);
        return -1;
    }
    return fd;

}
int main(){
    const Server& server = pick_server();
    int fd=connect_to_server(server);
    if (fd<0){
        perror("connect");
        return -1;
    }
    std::string request="GET / HTTP/1.1\r\nHost: dummy_host\r\nConnection: close\r\n\r\n";
    //send over tcp
    write(fd, request.c_str(),request.size());

    char buf[4096];
    ssize_t n;
    while(true){
        n=read(fd,buf,sizeof(buf));
        if (n<=0){
            break;
        }
        write(1,buf,n);
    }
}
