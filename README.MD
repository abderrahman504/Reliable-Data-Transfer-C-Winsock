# Socket Programming with C

This project is composed of two parts: A multithreaded web server that can handle requests, and a client that sends requests to a web server.
  
  
## Web Server
The server does not accept requests from a client until a connection is established first.  
When a connection is established, the server can process two types of HTTP requests: GET, and POST.  
A request can contain optional information

  
### Priting Requests
The server should print the request type followed by any optional info, then a blank line.  

### GET Request
The GET request requests a file residing in the server.  
If the file exists then the server should send an OK response followed by data from the file, then a blank line. If the file does not exist the the server should simply respond with a NOT FOUND response.  
  
### POST Request
When the server received a POST request, it immediately responds with an OK response and waits for a file to be sent from the client.
  
## Closing Connections
The server closes a connection after a certain amount of time has elapsed without any requests from the client.  
  
  
## Client
The client program reads from an input file the series of requests that it should send.
