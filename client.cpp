#include "common.hpp"
#include "zmq.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

typedef struct {
    int sockfd;
    struct sockaddr_in servaddr;
} sock_t;

typedef struct data {
    char buff[128];
    char retbuff[128];
} data_t;

enum ReqType { GET,
    SET,
    START,
    STOP };

struct AIOLimits {
    enum ReqType type;
    int maxVolt, minVolt, maxCurr, minCurr, maxPowerLim, peakCurrRipple;
};

data_t data = {
    .buff = "GET:12:32:32:55:21:77",
};

sock_t clisock;
struct AIOLimits AIODATA;
zmq::context_t contextReq, contextResp;
// sockets
zmq::socket_t zmqReqSocket(contextReq, ZMQ_DEALER);
zmq::socket_t zmqRespSocket(contextResp, ZMQ_DEALER);

void* m_event_serve_fun(void* args);
void processZmqMessage(std::string message);
int zmqReceiverThread();
void handle_sigint(int sig); 

int main(int argc, char* argv[])
{

    signal(SIGINT, handle_sigint); 

    // set socket identity
    zmq_setsockopt(zmqReqSocket, ZMQ_IDENTITY, "AIO_SERVICE",
        sizeof("AIO_SERVICE") - 1);
    zmq_setsockopt(zmqRespSocket, ZMQ_IDENTITY, "AIO_SERVICE",
        sizeof("AIO_SERVICE") - 1);
    // connect
    zmqReqSocket.connect(ZMQ_REQUEST_SOCKET_ADDRESS);
    zmqRespSocket.connect(ZMQ_RESPONSE_SOCKET_ADDRESS);
    std::thread msgReceiverThread = std::thread(zmqReceiverThread);
    msgReceiverThread.detach();
    int ret;
    printf("[DEBUG] addr:%s\tport:%d\n", argv[1], atoi(argv[2]));
    clisock.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clisock.sockfd < 0) {
        perror("[ERROR] sock-");
        exit(1);
    }
    bzero(&clisock.servaddr, sizeof(clisock.servaddr));
    clisock.servaddr.sin_family = AF_INET;
    clisock.servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    clisock.servaddr.sin_port = htons(atoi(argv[2]));

    ret = connect(clisock.sockfd, (struct sockaddr*)&clisock.servaddr,
        sizeof(clisock.servaddr));
    if (ret < 0) {
        perror("[ERROR] Unable to Connect:");
    } else {
        printf("[INFO] Client establised connection with server\n");
    }
    while (1) {
        sleep(8400);
    }
    int i = 0;
    while (1) {
        if (i == 0) {
            strcpy(data.buff, "GET:0:0:0:0:0:0");
        } else if (i == 1) {
            strcpy(data.buff, "SET:12:32:32:55:21:77");
        } else if (i == 2) {
            strcpy(data.buff, "START:12:32:32:55:21:77");
        } else if (i == 3) {
            strcpy(data.buff, "SET:0:12:1:20:50:70");
        }
        write(clisock.sockfd, (void*)&data.buff, sizeof(data.buff));
        recv(clisock.sockfd, (void*)&data.retbuff, sizeof(data.retbuff), 0);
        printf("[DEBUG] Data from server=%s\n", data.retbuff);
        sleep(1);
        i++;
    }
    printf("[INFO] Send and connection end\n");
    close(clisock.sockfd);
}

void processZmqMessage(std::string message)
{
    int status = FAILED;
    rapidjson::Document zmqMessageDocument;
    rapidjson::Document zmqReplyDoc;

    if (stringToJSON(message, zmqMessageDocument) == SUCCESS) {
        status = SUCCESS;
        switch (zmqMessageDocument["messagetype"].GetInt()) {
        case eZmqMessageType::REQUEST:
            switch (zmqMessageDocument["datatype"].GetInt()) {
            case eZmqDataType::AIO_LIMITS: {
                strcpy(data.buff, "GET:0:0:0:0:0:0");
                write(clisock.sockfd, (void*)&data.buff, sizeof(data.buff));
                recv(clisock.sockfd, (void*)&data.retbuff, sizeof(data.retbuff), 0);
                printf("[DEBUG] Data from server=%s\n", data.retbuff);
                sZmqPayload resp;
                std::string respStr;
                resp.source = eServiceList::AIO_SERVICE;
                resp.destination = eServiceList::ISO15118;
                resp.coRelationID = zmqMessageDocument["corelation"].GetInt();
                resp.messageType = eZmqMessageType::RESPONSE;
                resp.dataType = eZmqDataType::AIO_LIMITS;
                rapidjson::Document limits;
                limits.SetObject();
                limits.AddMember("MinVolt", AIODATA.minVolt, limits.GetAllocator());
                limits.AddMember("MaxVolt", AIODATA.maxVolt, limits.GetAllocator());
                limits.AddMember("MinCurr", AIODATA.minCurr, limits.GetAllocator());
                limits.AddMember("MaxCurr", AIODATA.maxCurr, limits.GetAllocator());
                limits.AddMember("MaxPower", AIODATA.maxPowerLim, limits.GetAllocator());
                limits.AddMember("PeakCurr", AIODATA.peakCurrRipple, limits.GetAllocator());
                resp.data = jsonStringify(limits);
                structToJSON(resp, respStr);
                sendZMQMessageFromDealerToRouter(zmqRespSocket, respStr);
                break;
            }
            default:
                break;
            }
            break;
        case eZmqMessageType::RESPONSE:
            break;
        default:
            break;
        }
    }
    return;
}


int zmqReceiverThread()
{
    zmq_pollitem_t zmqItems[] = {
        { zmqReqSocket, 0, ZMQ_POLLIN, 0 },
        { zmqRespSocket, 0, ZMQ_POLLIN, 0 },
    };

    while (true) {
        zmq_poll(zmqItems, 2, -1);
        if (zmqItems[0].revents & ZMQ_POLLIN) {
            zmq::message_t messageHandler;
            if (zmqReqSocket.recv(messageHandler, zmq::recv_flags::none) != FAILED) {
                std::string zmqMessage = messageHandler.to_string();
                std::cout << " AIO REQUEST SOCKET RECEIVED MESSAGE " << zmqMessage
                          << std::endl;
                std::thread([&, zmqMessage]() {
                    if (!zmqMessage.empty()) {
                        processZmqMessage(zmqMessage);
                    }
                }).detach();
            }
        }
        if (zmqItems[1].revents & ZMQ_POLLIN) {
            zmq::message_t messageHandler;
            if (zmqRespSocket.recv(messageHandler, zmq::recv_flags::none) != FAILED) {
                std::string zmqMessage = messageHandler.to_string();
                std::cout << " AIO RESPONSE SOCKET RECEIVED MESSAGE " << zmqMessage
                          << std::endl;
                /*                 std::thread([&, zmqMessage]() {
                                    if (!zmqMessage.empty()) {
                                        this->processZmqMessage(zmqMessage);
                                    }
                                }).detach(); */
            }
        }
    }

    return SUCCESS;
}

void* m_event_serve_fun(void* args)
{
    int i = 0;
    char* ch;
    char typebuff[128];
    strcpy(typebuff, data.buff);
    ch = strtok(typebuff, ":");
    printf("[DEBUG] In m_event_serve_fun func:%s\n", ch);
    if (strcmp(ch, "GET") == 0) {
        AIODATA.type = GET;
    } else if (strcmp(ch, "SET") == 0) {
        AIODATA.type = SET;
    } else if (strcmp(ch, "START") == 0) {
        AIODATA.type = START;
    } else if (strcmp(ch, "STOP") == 0) {
        AIODATA.type = STOP;
    }
    // TYPE:MinV:MaxV:MinC:MaxC:Power:PeakRipple
    switch (AIODATA.type) {
    case SET:
        ch = strtok(data.buff, ":");
        printf("[DEBUG] In SET\n");
        while (ch != NULL) {
            if (i == 0) {
                printf("[DEBUG] TYPE:%d\n", atoi(ch));
                // AIODATA.minVolt = atoi(ch);
            } else if (i == 1) {
                printf("[DEBUG] Min Voltage:%d\n", atoi(ch));
                AIODATA.minVolt = atoi(ch);
            } else if (i == 2) {
                printf("[DEBUG] Max Voltage:%d\n", atoi(ch));
                AIODATA.maxVolt = atoi(ch);
            } else if (i == 3) {
                printf("[DEBUG] Min Current:%d\n", atoi(ch));
                AIODATA.minCurr = atoi(ch);
            } else if (i == 4) {
                printf("[DEBUG] Max Current:%d\n", atoi(ch));
                AIODATA.maxCurr = atoi(ch);
            } else if (i == 5) {
                printf("[DEBUG] Max Power:%d\n", atoi(ch));
                AIODATA.maxPowerLim = atoi(ch);
            } else if (i == 6) {
                printf("[DEBUG] Peak current Ripple:%d\n", atoi(ch));
                AIODATA.peakCurrRipple = atoi(ch);
            }
            i++;
            ch = strtok(NULL, ":");
        }
        strcpy(data.retbuff, "DATA RECEIVED");
        break;
    case GET:
        printf("[DEBUG] In GET");
        strcpy(data.retbuff, "Data:330:650:8:130:35005:128");
        break;
    case START:
        strcpy(data.retbuff, "CHARGING START");
        printf("[DEBUG] In START");
        break;
    case STOP:
        strcpy(data.retbuff, "CHARGING STOP");
        printf("[DEBUG] In STOP");
        break;
    default:
        printf("[DEBUG] WRONG TYPE\n");
    }
    return NULL;
}

void handle_sigint(int sig) 
{ 
    printf("[DEBUG] EXIT\n");
	close(clisock.sockfd);
	exit(3);	
} 
