/*******************************************************
Protocolos de Transporte
Grado en Ingeniería Telemática
Dpto. Ingeníería de Telecomunicación
Univerisdad de Jaén

Fichero: servidor.c
Versión: 2.1
Fecha: 10/2020
Descripción:
	Servidor concurrente de eco sencillo TCP sobre IPv4/IPv6.

Autor: Juan Carlos Cuevas Martínez

******************************************************
* Alumno 1:
* Alumno 2:
*
******************************************************/
#include <stdio.h>		// Biblioteca estándar de entrada y salida
#include <ws2tcpip.h>	// Necesaria para las funciones IPv6
#include <process.h>	// Biblioteca para el empleo de hebras de ejecución
#include <locale.h>		// Para establecer el idioma de la codificación de texto, números, etc.
#include "protocol.h"	// Declarar constantes y funciones de la práctica

#pragma comment(lib, "Ws2_32.lib")

void servicio(void* socket);

int main(int* argc, char* argv[])
{
	WORD wVersionRequested;
	WSADATA wsaData;
	SOCKET sockfd, nuevosockfd;
	struct sockaddr* server_in = NULL;
	struct sockaddr* remote_addr = NULL;
	struct sockaddr_in server_in4;
	struct sockaddr_in6  server_in6;
	int address_size = sizeof(struct sockaddr_in6);
	char remoteaddress[128] = "";
	unsigned short remoteport = 0;

	int err;
	int ipversion = AF_INET;//IPv4 por defecto
	char ipdest[256];
	char opcion[256];

	//Inicialización de idioma
	setlocale(LC_ALL, "es-ES");

	/** INICIALIZACION DE BIBLIOTECA WINSOCK2 **
	 ** OJO!: SOLO WINDOWS                    **/
	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		return(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
		WSACleanup();
		return(-2);
	}
	/** FIN INICIALIZACION DE BIBLIOTECA WINSOCK2 **/
	printf("SERVIDOR> ¿Qué versión de IP desea usar?\r\n\t 6 para IPv6, 4 para IPv4 [por defecto] ");
	gets_s(opcion, sizeof(ipdest));

	if (strcmp(opcion, "6") == 0) {
		ipversion = AF_INET6;

	}
	else { //Distinto de 6 se elige la versión 4
		ipversion = AF_INET;
	}


	sockfd = socket(ipversion, SOCK_STREAM, IPPROTO_TCP);

	if (sockfd == INVALID_SOCKET) {
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-1);
	}
	else {
		if (ipversion == AF_INET6) {
			memset(&server_in6, 0, sizeof(server_in6));
			server_in6.sin6_family = AF_INET6; // Familia de protocolos IPv6 de Internet
			server_in6.sin6_port = htons(TCP_SERVICE_PORT);// Puerto del servidor
			//inet_pton(ipversion, "::1", &server_in6.sin6_addr);	// Direccion IP del servidor
																	// Se debe cambiar para que conincida con la de la interfaz
																	// del host que se quiera usar
			server_in6.sin6_addr = in6addr_any;//Conexiones de cualquier interfaz y de IPv4 o IPv6
			server_in6.sin6_flowinfo = 0;
			server_in = (struct sockaddr*) & server_in6;
			address_size = sizeof(server_in6);
		}
		else {
			//ipversion == AF_INET
			memset(&server_in4, 0, sizeof(server_in4));
			server_in4.sin_family = AF_INET; // Familia de protocolos IPv4 de Internet
			server_in4.sin_port = htons(TCP_SERVICE_PORT);// Puerto del servidor
			server_in4.sin_addr.s_addr = INADDR_ANY;
			//inet_pton(ipversion, "127.0.0.1", &server_in4.sin_addr.s_addr);//Dirección de loopback
			server_in = (struct sockaddr*) & server_in4;
			address_size = sizeof(server_in4);
		}
	}

	if (bind(sockfd, (struct sockaddr*)server_in, address_size) < 0) {
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-2);
	}

	if (listen(sockfd, 5) != 0) {
		DWORD error = GetLastError();
		printf("Error %d\r\n", error);
		return (-3);
	}

	// El servidor espera conexiones en un bucle infinito. 
	// Lo debe parar el administrador
	while (1) {
		printf("SERVIDOR> ESPERANDO NUEVA CONEXION DE TRANSPORTE\r\n");
		remote_addr = malloc(address_size);

		nuevosockfd = accept(sockfd, (struct sockaddr*)remote_addr, &address_size);
		if (nuevosockfd == INVALID_SOCKET) {
			DWORD error = GetLastError();
			printf("Error %d\r\n", error);

		}
		else {

			//Se comprueba si la dirección es IPv6 para mostrarla
			if (ipversion == AF_INET6) {
				struct sockaddr_in6* temp = (struct sockaddr_in6*)remote_addr;
				inet_ntop(AF_INET6, &(temp->sin6_addr), remoteaddress, sizeof(remoteaddress));
				remoteport = ntohs(temp->sin6_port);
			}
			else {
				//Si no es IPv6 se supone IPv4
				struct sockaddr_in* temp = (struct sockaddr_in*)remote_addr;
				inet_ntop(AF_INET, &(temp->sin_addr), remoteaddress, sizeof(remoteaddress));
				remoteport = ntohs(temp->sin_port);
			}

			printf("SERVIDOR> CLIENTE CONECTADO DESDE %s:%u\r\n", remoteaddress, remoteport);

			_beginthread(servicio, 0, (void*)&nuevosockfd);//Se le pasa a la hebra de atención el nuevo socket
		}

	}

	printf("SERVIDOR> CERRANDO SERVIDOR\r\n");

	return(0);
}

/*
 Función que incorpora el código de atención a cada cliente que se ejecuta en una hebra
*/
void servicio(void* socket) {
	SOCKET* nuevosockfd;
	char buffer_out[1024], buffer_in[1024], cmd[10], usr[10], pas[10];
	int fin = 0, fin_conexion = 0;
	int recibidos = 0, enviados = 0;
	int estado = 0;
	SOCKADDR remote;
	struct sockaddr_in* remote4;
	struct sockaddr_in6* remote6;
	int remote_len = sizeof(remote);
	int port = 0;
	char remote_addr[1024];

	nuevosockfd = (SOCKET*)socket;

	//Se obtiene la dirección del otro extremo
	getpeername(*nuevosockfd, &remote, &remote_len);

	//Se comprueba si es IPv4 o IPv6
	if (remote.sa_family == AF_INET) {
		remote4 = (struct sockaddr_in*) & remote;
		inet_ntop(AF_INET, &(remote4->sin_addr), remote_addr, sizeof(remote_addr));
		port = ntohs(remote4->sin_port);

	}
	else {
		remote6 = (struct sockaddr_in6*) & remote;
		inet_ntop(AF_INET6, &(remote6->sin6_addr), remote_addr, sizeof(remote_addr));
		port = ntohs(remote6->sin6_port);
	}

	//Mensaje de Bienvenida
	sprintf_s(buffer_out, sizeof(buffer_out), "%s Bienvenido al servidor Sencillo%s", OK, CRLF);

	enviados = send(*nuevosockfd, buffer_out, (int)strlen(buffer_out), 0);

	//Se reestablece el estado inicial
	estado = S_USER;
	fin_conexion = 0;

	printf("SERVIDOR [CLIENTE EN %s:%d]> Esperando conexion de aplicacion\r\n", remote_addr, port);
	do {
		//Se espera un comando del cliente
		recibidos = recv(*nuevosockfd, buffer_in, 1023, 0);

		buffer_in[recibidos] = 0x00;// Dado que los datos recibidos se tratan como cadenas
									// se debe introducir el carácter 0x00 para finalizarla
									// ya que es así como se representan las cadenas de caracteres
									// en el lenguaje C

		printf("SERVIDOR RECIBIDO [%s:%d %d bytes]>%s\r\n", remote_addr,port,recibidos, buffer_in);

		//SE analiza el formato de la PDU de aplicación (APDU)
		strncpy_s(cmd, sizeof(cmd), buffer_in, 4);
		cmd[4] = 0x00; // Se finaliza la cadena
		printf("SERVIDOR COMANDO RECIBIDO>%s\r\n", cmd);

		//Máquina de estados del servidor para seguir el protocolo
		//En función del estado en el que se encuentre habrá unos comandos permitidos
		switch (estado) {
		case S_USER:
			if (strcmp(cmd, SC) == 0) { // si recibido es solicitud de conexion de aplicacion

				sscanf_s(buffer_in, "USER %s\r\n", usr, sizeof(usr));

				// envia OK acepta todos los usuarios hasta que tenga la clave
				sprintf_s(buffer_out, sizeof(buffer_out), "%s%s", OK, CRLF);

				estado = S_PASS;
				printf("SERVIDOR> Esperando clave\r\n");
			}
			else
				if (strcmp(cmd, SD) == 0) {
					sprintf_s(buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK, CRLF);
					fin_conexion = 1;
				}
				else {
					sprintf_s(buffer_out, sizeof(buffer_out), "%s Comando incorrecto%s", ER, CRLF);
				}
			break;

		case S_PASS:
			if (strcmp(cmd, PW) == 0) { // si comando recibido es password

				sscanf_s(buffer_in, "PASS %s\r\n", pas, sizeof(usr));

				if ((strcmp(usr, USER) == 0) && (strcmp(pas, PASSWORD) == 0)) { // si password recibido es correcto
					// envia aceptacion de la conexion de aplicacion con el nombre de usuario
					sprintf_s(buffer_out, sizeof(buffer_out), "%s %s%s", OK, usr, CRLF);
					estado = S_DATA;
					printf("SERVIDOR> Esperando comando\r\n");
				}
				else {
					sprintf_s(buffer_out, sizeof(buffer_out), "%s Autenticación errónea%s", ER, CRLF);
					estado = S_USER;//Volvemos al estado S_USER para reiniciar la autenticación
				}
			}
			else if (strcmp(cmd, SD) == 0) {
				sprintf_s(buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK, CRLF);
				fin_conexion = 1;
			}
			else {
				sprintf_s(buffer_out, sizeof(buffer_out), "%s Comando incorrecto%s", ER, CRLF);
			}
			break;

		case S_DATA: 
			buffer_in[recibidos] = 0x00;

			if (strcmp(cmd, SD) == 0) {
				sprintf_s(buffer_out, sizeof(buffer_out), "%s Fin de la conexión%s", OK, CRLF);
				fin_conexion = 1;
			}
			else if (strcmp(cmd, ECHO) == 0) {
				char echo[1024];
				sscanf_s(buffer_in, "ECHO %[^\r]\r\n", echo, sizeof(echo));// la expresión %[^\r] hace que se busque
																		   // se añada a echo cualquier carácter que no
																		   // sea \r (CR)
				sprintf_s(buffer_out, sizeof(buffer_out), "%s %s%s", OK, echo, CRLF);
			}
			else {
				sprintf_s(buffer_out, sizeof(buffer_out), "%s Comando incorrecto: %s%s", ER, cmd, CRLF);
			}
			break;

		default:
			break;

		} // switch
		//RAMA PRUEBA

		enviados = send(*nuevosockfd, buffer_out, (int)strlen(buffer_out), 0);//Aquí hay un error sin controlar

	} while (!fin_conexion);

	printf("SERVIDOR> CERRANDO CONEXION DE TRANSPORTE\r\n");
	shutdown(*nuevosockfd, SD_SEND);
	closesocket(*nuevosockfd);

}