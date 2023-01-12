/*
 * Asynchronous Web Server - header file (macros and structures)
 *
 * 2011-2017, Operating Systems
 */

#ifndef AWS_H_
#define AWS_H_		1

#ifdef __cplusplus
extern "C" {
#endif

#define AWS_LISTEN_PORT		8888
#define AWS_DOCUMENT_ROOT	"./"
#define AWS_REL_STATIC_FOLDER	"static/"
#define AWS_REL_DYNAMIC_FOLDER	"dynamic/"
#define AWS_ABS_STATIC_FOLDER	(AWS_DOCUMENT_ROOT AWS_REL_STATIC_FOLDER)
#define AWS_ABS_DYNAMIC_FOLDER	(AWS_DOCUMENT_ROOT AWS_REL_DYNAMIC_FOLDER)

# define ERROR_MSG													\
	"HTTP/1.0 404 Not Found\r\n\r\n"

# define OK_MSG													\
	"HTTP/1.0 200 OK\r\n"										\
    "Date: Thu, 03 Jun 2010 09:56:32 GMT\r\n"					\
    "Server: Apache/2.2.3 (Red Hat)\r\n"						\
    "Cache-Control: public\r\n"									\
    "Pragma: \r\n"												\
    "Location: http://www.bonjourmadame.fr/\r\n"				\
    "Vary: Accept-Encoding\r\n"									\
    "Content-Type: text/html; charset=UTF-8\r\n"				\
    "Connection: close\r\n"										\
    "Content-Length: "									

#ifdef __cplusplus
}
#endif

#endif /* AWS_H_ */
