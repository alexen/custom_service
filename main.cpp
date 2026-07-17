/// getpid()
#include <unistd.h>

/// POSIX Network
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>
#include <csignal>
#include <iostream>
#include <iomanip>
#include <functional>
#include <thread>
#include <chrono>
#include <memory>


using namespace std::string_literals;


void reportAppStarting( int argc, char** argv )
{
     std::cout
          << "Start application " << std::quoted( argv[ 0 ] ) << " (" << getpid() << ")"
          << (argc > 1 ? " with args:\n" : " without args\n");
     for( auto i = 1; i < argc; ++i )
     {
          std::cout << std::setw( 4 ) << i << ") " << argv[ i ] << '\n';
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
     for( auto&& signum: signums )
     {
          std::cout << "Set handler for signal " << SigNum{ signum } << '\n';
          signal( signum, handler );
     }
}


#define THROW_POSIX_ERROR( fn ) \
     do { \
          throw std::runtime_error{ \
               std::string{ fn } + " failed: "s + strerror( errno ) \
          }; \
     } while( false )



void runServerOnPort( std::uint16_t port )
{
     std::cout << "Start listening port " << port << '\n';

     // 1. Создаём сокет (IPv4, TCP)
     const int server_fd = socket( AF_INET, SOCK_STREAM, 0 );
     if( server_fd == -1 )
     {
          THROW_POSIX_ERROR( "socket()" );
     }

     std::shared_ptr< const int > srvFdGrd{ &server_fd, []( auto fd ){ close( *fd ); } };

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

     while( true )
     {
          sockaddr_in client_addr {};
          socklen_t client_len = sizeof( client_addr );

          const auto client_fd = accept( server_fd, (sockaddr *) &client_addr, &client_len );
          if( client_fd == -1 )
          {
               THROW_POSIX_ERROR( "accept()" );
          }

          std::shared_ptr< const int > clnFd{ &client_fd, []( auto fd ){ close( *fd ); } };

          std::cout
               << "Client connected: "
               << inet_ntoa(client_addr.sin_addr) << ":"
               << ntohs(client_addr.sin_port) << std::endl;

          static char buffer[ 2048u ];
          static char reversed[ sizeof( buffer ) ];

          // Читаем данные от клиента
          const auto bytes = recv( client_fd, buffer, sizeof( buffer ) - 1, 0 );
          if( bytes > 0 )
          {
               std::cout
                    << "Recv[" << bytes << "]: " << std::string_view( buffer, bytes )
                    << '\n';

               std::copy(
                    std::make_reverse_iterator( buffer + bytes ),
                    std::make_reverse_iterator( buffer ),
                    reversed
               );

               const auto rv = send( client_fd, reversed, bytes, 0 );

               std::cout
                    << "Send[" << rv << "]: " << std::string_view( reversed, bytes )
                    << '\n';
          }
          else if( bytes == 0 )
          {
               std::cout << "Client disconnected!\n";
          }
          else
          {
              THROW_POSIX_ERROR( "recv()" );
          }
     }
}


void signalHandler( int signum )
{
     std::cout << "Caught signal " << SigNum{ signum } << "!\n";
     signal( signum, SIG_DFL );
     raise( signum );
}


int main( int argc, char** argv )
{
     try
     {
          reportAppStarting( argc, argv );

          setSignalsHandler( { SIGINT, SIGTERM, SIGQUIT }, signalHandler );

          runServerOnPort( 8080 );
     }
     catch( const std::exception& e )
     {
          std::cerr << "exception: " << e.what() << '\n';
          return 1;
     }
     return 0;
}
