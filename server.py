import BaseHTTPServer
import CGIHTTPServer
import cgitb; cgitb.enable()  ## This line enables CGI error reporting
import os
import os.path

class MyHandler(CGIHTTPServer.CGIHTTPRequestHandler):
    def do_PUT(self):
        """
        Serve a PUT request.
        This is only implemented for CGI scripts.
        """
        if self.is_cgi():
            self.run_cgi()
        else:
            self.send_error(501, "Can only PUT to CGI scripts")

# Go to previous dir to simulate being on the host, and have the "notera"
# folder on the URL when accessing the server. e.g., instead of
# http://host/cgi-bin/api.cgi, have http://host.com/notera/cgi-bin/api.cgi.
os.chdir("..")

server = BaseHTTPServer.HTTPServer
handler = MyHandler
handler.cgi_directories = ["/notera/cgi-bin"]
server_address = ("", 8000)

httpd = server(server_address, handler)
httpd.serve_forever()

