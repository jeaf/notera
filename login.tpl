<html>
    <head>
        <script>
            function submit_login()
            {
                frm = document.login_form

                // Get the salt for the user before doing the login
                $.get("main.exe", {api: "salt", user: frm.username.value},
                      function(data)
                      {
                          frm.auth_token.value = CryptoJS.SHA1(frm.auth_token.value + data.salt)
                          frm.auth_token.value = CryptoJS.SHA1(frm.username.value + frm.auth_token.value + $.cookie("sid"))
                          frm.submit()
                      });                
            }
        </script>
    </head>
    <body>
        <form name="login_form" action="main.exe?login=1" method="post">
            Username: <input type="text"     name="username"><br/>
            Password: <input type="password" name="auth_token"><br/>
            <input type="button" value="Login" onClick="submit_login()" />
        </form>
        <script src="http://ajax.googleapis.com/ajax/libs/jquery/2.0.3/jquery.min.js"></script>
        <script src="http://cdnjs.cloudflare.com/ajax/libs/jquery-cookie/1.3.1/jquery.cookie.min.js"></script>
        <script src="http://crypto-js.googlecode.com/svn/tags/3.1.2/build/rollups/sha1.js"></script>
    </body>
</html>
