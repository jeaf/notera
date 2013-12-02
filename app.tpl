<!DOCTYPE html>
<html>
    <head>
        <script src="http://cdnjs.cloudflare.com/ajax/libs/marked/0.2.9/marked.min.js"></script>
        <script src="http://cdnjs.cloudflare.com/ajax/libs/highlight.js/7.3/highlight.min.js"></script>
    </head>
    <body>
        <textarea id="text-input" oninput="this.editor.update()"
                  rows="6" cols="60">Type **Markdown** here.</textarea>
        <div id="preview"> </div>
        <script>
          marked.setOptions({
            highlight: function (code, lang) {
              return hljs.highlightAuto(lang, code).value;
            }
          });
          function Editor(input, preview) {
            this.update = function () {
              preview.innerHTML = marked(input.value);
            };
            input.editor = this;
            this.update();
          }
          var $ = function (id) { return document.getElementById(id); };
          new Editor($("text-input"), $("preview"));
        </script>
    </body>
</html>

