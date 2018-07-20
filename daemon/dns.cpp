#include <signal.h>
#include <getopt.h>
#include <stdio.h>		/* fprintf, printf */

#include "net.hpp"
#include "logger.hpp"
#include "ev.hpp"
#include <llarp/logic.h>
#include "dnsd.hpp"

#include <vector>
#include <thread> // for multithreaded version

bool done = false;

void
handle_signal(int sig)
{
  printf("got SIGINT\n");
  done = true;
}

// FIXME: make configurable
#define SERVER "8.8.8.8"
#define PORT 53

int
main(int argc, char *argv[])
{
  int code = 1;
  llarp::LogInfo("Starting up server");

  //bllarp::SetLogLevel(llarp::eLogDebug);
  
  if (1)
  {
    // libev version
    llarp_ev_loop *netloop  = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;
    
    llarp_ev_loop_alloc(&netloop);
    
    struct dnsd_context dnsd;
    if (!llarp_dnsd_init(&dnsd, netloop, "*", 53, SERVER, PORT))
    {
      llarp::LogError("Couldnt init dns daemon");
      return 0;
    }
    
    /*
    // configure main netloop
    llarp_udp_io *udp = new llarp_udp_io;

    llarp::Addr p_addr;
    sockaddr_in ip4addr;
    sockaddr *addr = nullptr;
    addr = (sockaddr *)&ip4addr; // point addr to ip4addr
    llarp::Zero(addr, sizeof(sockaddr_in)); // zero out ip4addr
    addr->sa_family = AF_INET;

    // FIXME: make configureable
    ip4addr.sin_port = htons(1053);
    p_addr = *addr; // copy addr into llarp address
    udp->user     = nullptr;
    udp->tick     = nullptr;
    udp->recvfrom = &llarp_handle_recvfrom;
    llarp::LogDebug("bind DNS Server to ", addr);
    if(llarp_ev_add_udp(netloop, udp, p_addr) == -1)
    {
      llarp::LogError("failed to bind to ", addr);
      return false;
    }
    */
    
    //struct dnsc_context *dns_clinet_context = new dnsc_context;
    //llarp_dnsc_init(dns_clinet_context, netloop, SERVER, PORT);
    
    // singlethreaded
    if (1)
    {
      llarp::LogInfo("singlethread start");
      worker = llarp_init_same_process_threadpool();
      logic = llarp_init_single_process_logic(worker);
      llarp_ev_loop_run_single_process(netloop, worker, logic);
      llarp::LogInfo("singlethread end");
    }
    else
    {
      llarp::LogInfo("multithreaded start");
      // create 2 workers
      worker = llarp_init_threadpool(2, "llarp-worker");
      logic = llarp_init_logic();
      auto netio = netloop;
      int num_nethreads   = 2;
      std::vector< std::thread > netio_threads;
      while(num_nethreads--)
      {
        netio_threads.emplace_back([netio]() { llarp_ev_loop_run(netio); });
#if(__APPLE__ && __MACH__)
        
#elif(__FreeBSD__)
        pthread_set_name_np(netio_threads.back().native_handle(),
                            "llarp-netio");
#else
        pthread_setname_np(netio_threads.back().native_handle(), "llarp-netio");
#endif
      }
      llarp_logic_mainloop(logic);
      llarp::LogInfo("multithreaded end");
    }
    llarp_ev_loop_free(&netloop);
  }
  else
  {
    
    struct sockaddr_in m_address;
    int m_sockfd;
    
    m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    m_address.sin_family = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port = htons(1053);
    int rbind = bind(m_sockfd, (struct sockaddr *) & m_address,
                     sizeof (struct sockaddr_in));
    
    if (rbind != 0) {
      llarp::LogError("Could not bind: ", strerror(errno));
      return 0;
    }
    
    const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE]; // 1024 is buffer size
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof (struct sockaddr_in);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000; // 1 sec
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
      perror("Error");
    }
    
    signal(SIGINT, handle_signal);
    while(!done)
    {
      // sigint quits after next packet
      int nbytes = recvfrom(m_sockfd, buffer, BUFFER_SIZE, 0,
                            (struct sockaddr *) &clientAddress, &addrLen);
      if (nbytes == -1) continue;
      llarp::LogInfo("Received Bytes ", nbytes);

      raw_handle_recvfrom(&m_sockfd, (const struct sockaddr *)&clientAddress, buffer, nbytes);
    }
  }

  return code;
}
