#include "markdown.h"
#include "html.h"
#include "buffer.h"

#include <stdio.h>
#include <string.h>

int main()
{
    printf("Content-type: text/html\n\n");

    char* test_md = "abc\n===\n\n* a\n* b\n";

    struct sd_callbacks callbacks;
    struct html_renderopt options;
    sdhtml_renderer(&callbacks, &options, 0);
    struct sd_markdown* md = sd_markdown_new(0, 16, &callbacks, &options);
    struct buf* ob = bufnew(64);
    sd_markdown_render(ob, test_md, strlen(test_md), md);
    
    printf("abc\n");
    printf("%s\n", ob->data);
    return 0;
}

