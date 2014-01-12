<html>
    <head>
        <script>
            function submit_new_account()
            {
                frm = document.new_account_form

                // Get the salt for the user before creating the new account
                $.get("main.exe", {api: "salt", user: frm.username.value},
                      function(data)
                      {
                          frm.pwd_hash.value = CryptoJS.SHA1(frm.pwd_hash.value + data.salt)
                          frm.submit()
                      });                
            }
        </script>
    </head>
    <body>
        <form name="new_account_form" action="main.exe?submit_new_account=1" method="post">
            Username: <input type="text"     name="username"><br/>
            Password: <input type="password" name="pwd_hash"><br/>
            <input type="button" value="new_account" onClick="submit_new_account()" />
        </form>
        <script src="http://ajax.googleapis.com/ajax/libs/jquery/2.0.3/jquery.min.js"></script>
        <script src="http://crypto-js.googlecode.com/svn/tags/3.1.2/build/rollups/sha1.js"></script>
    </body>
</html>

