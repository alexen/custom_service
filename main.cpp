#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstdint>
#include <atomic>

#include <boost/log/trivial.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/throw_exception.hpp>


using namespace std::string_literals;


#define THROW_POSIX_ERROR( fn ) \
     do { \
          BOOST_THROW_EXCEPTION( std::runtime_error( \
               std::string{ fn } + " failed: "s + strerror( errno ) \
          ) ); \
     } while( false )


void reportAppStarting( int argc, char** argv )
{
     BOOST_LOG_TRIVIAL( info )
          << "Start application " << std::quoted( argv[ 0 ] ) << " (pid: " << getpid() << ")"
          << (argc > 1 ? " with args:" : " without args");
     for( auto i = 1; i < argc; ++i )
     {
          BOOST_LOG_TRIVIAL( info ) << std::setw( 3 ) << i << ") " << argv[ i ];
     }
}


struct SigNum {
     explicit SigNum( int signum ) : value{ signum } {}
     const int value {};
};


std::ostream& operator<<( std::ostream& os, const SigNum& sn )
{
     return os << std::quoted( strsignal( sn.value ) );
}


void setSignalsHandler( std::initializer_list< int > signums, __sighandler_t handler )
{
     /// ВАЖНО: флаг SA_RESTART отсутствует
     struct sigaction sa {};
     sa.sa_handler = handler;
     sigemptyset( &sa.sa_mask );

     for( auto&& signum: signums )
     {
          BOOST_LOG_TRIVIAL( info ) << "Set handler for signal " << SigNum{ signum };
          if( sigaction( signum, &sa, nullptr ) == -1 )
          {
               THROW_POSIX_ERROR( "sigaction" );
          }
     }
}


// Функция запрашивает UID владельца TCP-сокета напрямую у ядра Linux через Netlink
uid_t getTcpSocketUserId( std::uint16_t remotePort )
{
     BOOST_LOG_TRIVIAL( trace ) << "[TRACE] >>> STARTING BI-DIRECTIONAL NETLINK DIAGNOSTIC SCAN <<<";
     BOOST_LOG_TRIVIAL( trace ) << "[TRACE] Incoming Client Port (Remote): " << remotePort;

     const auto netlinkFd = socket( AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG );
     if( netlinkFd < 0 )
     {
          BOOST_LOG_TRIVIAL( trace )
               << "[TRACE] FAILED: socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG) failed: "
               << strerror( errno );
          return -1;
     }

     struct timeval tv {};
     tv.tv_sec = 0;
     tv.tv_usec = 500000; // 500мс таймаут
     if( setsockopt( netlinkFd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof( tv ) ) < 0 )
     {
          BOOST_LOG_TRIVIAL( trace )
               << "[TRACE] FAILED: setsockopt(netlinkFd, SOL_SOCKET, SO_RCVTIMEO) failed: "
               << strerror( errno );
          close( netlinkFd );
          return -1;
     }

     struct
     {
          struct nlmsghdr nlh {};
          struct inet_diag_req_v2 req {};
     }
     request {};

     request.nlh.nlmsg_len = sizeof( request );
     request.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;          /// Тип запроса для sock_diag
     request.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;  /// Дампим таблицу для фильтрации

     request.req.sdiag_family = AF_INET;          /// Ищем IPv4
     request.req.sdiag_protocol = IPPROTO_TCP;    /// Ищем TCP сокеты
     request.req.idiag_states = 0xFFFFFFFF;       /// Все состояния сокетов

     if( send( netlinkFd, &request, sizeof( request ), 0 ) < 0 )
     {
          BOOST_LOG_TRIVIAL( trace ) << "[TRACE] FAILED: send failed: " << strerror( errno );
          close( netlinkFd );
          return -1;
     }

     /// Выделяем 128КБ под системную таблицу ядра Linux
     std::vector< char > buffer( 128u * 1024u );
     uid_t verifiedClientUid = -1;
     bool scanComplete = false;
     int inspectedRowsCount = 0;

     while( !scanComplete )
     {
          auto nBytes = recv( netlinkFd, buffer.data(), buffer.size(), 0 );
          if( nBytes <= 0 )
          {
               BOOST_LOG_TRIVIAL( trace ) << "[TRACE] Kernel stream ended or timed out.";
               break;
          }

          for( auto nlh = (struct nlmsghdr*) buffer.data();
               NLMSG_OK( nlh, nBytes );
               nlh = NLMSG_NEXT( nlh, nBytes ) )
          {
               if( nlh->nlmsg_type == NLMSG_DONE
                   || nlh->nlmsg_type == NLMSG_ERROR )
               {
                    scanComplete = true;
                    break;
               }

               if( nlh->nlmsg_type == SOCK_DIAG_BY_FAMILY )
               {
                    auto diagMsg = (struct inet_diag_msg*) NLMSG_DATA( nlh );

                    auto kernelRowSport = ntohs( diagMsg->id.idiag_sport );
                    auto kernelRowDport = ntohs( diagMsg->id.idiag_dport );
                    auto kernelRowUid = diagMsg->idiag_uid;

                    inspectedRowsCount++;

                    /// Этот блок if оставляем здесь только для красивого
                    /// и информативного TRACE-вывода
                    if( kernelRowSport == remotePort
                        || kernelRowDport == remotePort )
                    {
                         verifiedClientUid = kernelRowUid;
                         scanComplete = true;

                         BOOST_LOG_TRIVIAL( trace )
                              << "        -> [ROW MATCH #" << inspectedRowsCount
                              << "]"
                              << " Kernel_Source_Port: " << kernelRowSport
                              << " | Kernel_Dest_Port: " << kernelRowDport
                              << " | Row_Owner_UID: " << kernelRowUid;
                    }

                    if( kernelRowSport == remotePort )
                    {
                         verifiedClientUid = kernelRowUid;

                         BOOST_LOG_TRIVIAL( trace ) << "           >>> TARGET CLIENT SOCKET DETECTED! <<<";
                         BOOST_LOG_TRIVIAL( trace ) << "               Validated Browser Process UID: " << verifiedClientUid;

                         scanComplete = true;
                         break;
                    }
               }
          }
     }

     close( netlinkFd );
     return verifiedClientUid;
}


bool isConnectionAllowed( const std::uint16_t remotePort )
{
     const auto clientUid = getTcpSocketUserId( remotePort );
     if( clientUid == static_cast< uid_t >( -1 ) )
     {
          BOOST_LOG_TRIVIAL( trace ) << "[TRACE] Netlink query failed to extract UID!";
          return false;
     }

     const auto currentUid = getuid();

     BOOST_LOG_TRIVIAL( trace )
          << "[TRACE] Comparing client UID " << clientUid
          << " with service UID " << currentUid;

     // Если UID совпали — это гарантированно один и тот же пользователь ОС
     return clientUid == currentUid;
}


std::atomic_bool running = true;


void runServerOnPort( std::uint16_t port )
{
     BOOST_LOG_TRIVIAL( info ) << "Start listening port " << port;

     // 1. Создаём сокет (IPv4, TCP)
     const int server_fd = socket( AF_INET, SOCK_STREAM, 0 );
     if( server_fd == -1 )
     {
          THROW_POSIX_ERROR( "socket()" );
     }

     // 1.1. Опционально: разрешить повторное использование адреса (удобно при перезапуске)
     int opt = 1;
     if( setsockopt( server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) ) == -1 )
     {
          THROW_POSIX_ERROR( "setsockopt()" );
     }

     sockaddr_in addr {};

     // 2. Настраиваем адрес: любой интерфейс, нужный порт
     memset( &addr, 0, sizeof(addr) );
     addr.sin_family = AF_INET;
     addr.sin_addr.s_addr = INADDR_ANY; // слушать на всех интерфейсах
     addr.sin_port = htons( port );     // порт в сетевом порядке байт

     // 3. Привязываем сокет к адресу и порту
     if( bind( server_fd, (sockaddr *) &addr, sizeof(addr) ) == -1 )
     {
          THROW_POSIX_ERROR( "bind()" );
     }

     // 4. Начинаем слушать входящие соединения
     static const auto BACKLOG = 5;
     if( listen( server_fd, BACKLOG ) == -1 )
     {
          THROW_POSIX_ERROR( "listen()" );
     }

     while( running )
     {
          sockaddr_in client_addr {};
          socklen_t client_len = sizeof( client_addr );

          const auto client_fd = accept( server_fd, (sockaddr *) &client_addr, &client_len );
          if( client_fd == -1 )
          {
               if( errno == EINTR )
               {
                    continue;
               }
               THROW_POSIX_ERROR( "accept()" );
          }

          const std::uint16_t remotePort = ntohs( client_addr.sin_port );

          BOOST_LOG_TRIVIAL( info )
               << "Client connected: "
               << inet_ntoa( client_addr.sin_addr )
               << ":" << remotePort;

          if( !isConnectionAllowed( remotePort ) )
          {
               BOOST_LOG_TRIVIAL( info ) << "Connection is not allowed!";
               shutdown( client_fd, SHUT_RD );
               close( client_fd );
               continue;
          }

          static char buffer[ 2048u ];
          static char reversed[ sizeof( buffer ) ];

          // Читаем данные от клиента
          const auto bytes = recv( client_fd, buffer, sizeof( buffer ) - 1, 0 );
          if( bytes > 0 )
          {
               BOOST_LOG_TRIVIAL( info )
                    << "Recv[" << bytes << "]: " << std::string_view( buffer, bytes );

               std::copy(
                    std::make_reverse_iterator( buffer + bytes ),
                    std::make_reverse_iterator( buffer ),
                    reversed
               );

               const auto rv = send( client_fd, reversed, bytes, 0 );

               BOOST_LOG_TRIVIAL( info )
                    << "Send[" << rv << "]: " << std::string_view( reversed, bytes );
          }
          else if( bytes == 0 )
          {
               BOOST_LOG_TRIVIAL( info ) << "Client disconnected!";
          }
          else
          {
               if( errno == EINTR )
               {
                    continue;
               }
               THROW_POSIX_ERROR( "recv()" );
          }
     }

     close( server_fd );
     BOOST_LOG_TRIVIAL( info ) << "Gracefully shutdown server and exit!";
}


void signalHandler( int signum )
{
     BOOST_LOG_TRIVIAL( debug ) << "Caught signal " << SigNum{ signum } << ": disable running flag!";
     running = false;
}


int main( int argc, char** argv )
{
     try
     {
          reportAppStarting( argc, argv );

          setSignalsHandler( { SIGINT, SIGTERM, SIGQUIT }, signalHandler );

          runServerOnPort( 8080 );
     }
     catch( ... )
     {
          BOOST_LOG_TRIVIAL( error )
               << boost::current_exception_diagnostic_information();
          return 1;
     }
     return 0;
}
