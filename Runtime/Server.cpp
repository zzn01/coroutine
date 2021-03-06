#include "Server.hpp"
#include "Logger.hpp"

using namespace Runtime;
Scheduler Server::scheduler(8);

Server::Server(short port, const char *ip)
    :tcp_serv_(poller_, port, ip), timer_serv_(poller_)
{

}

void Server::Loop(){
    poller_.Loop();
}

Server::~Server(){

}


TCPService::TCPService(Poller &p, short port, const char *ip)
    :tcp_server_(port,ip), poller_(p)
{
    poller_.Add(&tcp_server_,Event::POLLET|Event::POLLIN|Event::POLLRDHUP);
    tcp_server_.msg_handler = ParseMsg;
}

TCPService::~TCPService(){

}

void TCPService::ParseMsg(Runtime::TCPConn &, ReadBuf &buf) {
    Protocol *header = NULL;
    while(true){
        log_debug("parse msg handler stat:%d\n",buf.state);
        switch (buf.state){
        case Runtime::BufState::HEADER_WAITING:
            if (buf.top >= sizeof(Protocol)){
                header =(Protocol *) &buf.data[0];

                header->len = ntohl(header->len);
                header->channel = ntohl(header->channel);
                buf.state=Runtime::BufState::DATA_WAITING;

                log_debug("leading msg len:%d\n",header->len);

            }else{
                return;
            }

            break;
        case Runtime::BufState::DATA_WAITING:
            // XXX:必须每次获取header指针，防止resize改变了地址。
            header = (Protocol *) &buf.data[0];
            log_debug("buf len:%d, %d, %d\n", buf.top, sizeof(Protocol) , header->len);
            if (buf.top >= sizeof(Protocol) + header->len){
                buf.state=Runtime::BufState::DONE;
                int len = header->len;
                ChannelId chl_id = header->channel;

                std::string p(buf.data, sizeof(Protocol), len);

                buf.data.erase(0,sizeof(Protocol) + len);
                buf.top -= sizeof(Protocol) + len;
                log_debug("get a full message\n");

                // TODO:do somethig with packet.
                Channel<std::string> * chl = (Channel<std::string> *)ChannelMap[chl_id];
                if (chl == NULL){
                    log_warn("channel (%d) is not existed! drop message\n",chl_id);
                }else{
                    chl->Put(p);
                }
            }else{
                return;
            }

            break;

        case Runtime::BufState::DONE:
            buf.state = Runtime::BufState::HEADER_WAITING;
            break;
        }
    }
}


TimerService::TimerService(Poller &p)
    :poller_(p)
{

    timer_.timeout_handler = OnTimeout;
    poller_.Add(&timer_, Event::POLLIN|Event::POLLET);
}

TimerService::~TimerService(){

}


void TimerService::Sleep(time_t sec, long msec){

    sec += msec/1000;
    msec = msec%1000;

    TimeSpec t(sec, msec*1000);

    Task *task = CPU::current_core->running_task;
    timer_.SetTimer(t, task);

    Yield();
}

void TimerService::OnTimeout(const TimeSpec &, Task *const &t){
    log_debug("Task :%p time out\n",t);
    t->SetReady();
}




#ifdef __SERVER_UNITTEST
#include <stdlib.h>

Logger Runtime::log;

typedef Channel<std::string> StringChannel;
static StringChannel *GetChannel(int chl_id){
    StringChannel *chl=(StringChannel*) ChannelMap[chl_id];
    if (chl == NULL){
        chl = new StringChannel;
        ChannelMap[chl_id]= chl;
    }
    return chl;
}

int main(int argc, char **argv){
    short port = 2222;
    if (argc>1){
        port = atoi(argv[1]);
    }
    Server s(port);

    Server::scheduler.Spawn([&s](void *){
                log_debug("before sleep\n");
                s.Sleep(2);
                s.Sleep(3);
                log_debug("after sleep\n");
            });
    /*
    StringChannel *c = GetChannel(1);
    Server::scheduler.Spawn([c](void *){
            std::string s;
            while(true){
                s = c->Get();

                printf("1.Task:%s \n",s.c_str());
            }

            });

    Server::scheduler.Spawn([c](void *){
            std::string s;
            while(true){

                s = c->Get();

                printf("2.Task:%s \n",s.c_str());
            }

            });

    Server::scheduler.Spawn([c](void *){
            std::string s;
            for(int i = 0 ; i < 10; ++i){
                 s = "from routine 3";
                 *c<< s;
                 sleep(1);
                 //printf("3.Task:%s \n",s.c_str());
            }

            });

            */
    s.Loop();
}

#endif// __SERVER_UNITTEST

