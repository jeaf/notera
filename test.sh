curl -b cookies.txt -c cookies.txt -i -X GET "http://localhost:8000/notera/cgi-bin/api.exe?p1=session"
curl -b cookies.txt -c cookies.txt -i -X PUT "http://localhost:8000/notera/cgi-bin/api.exe?p1=session&p2=abc"

