#include <libxml/SAX.h>
#include <string.h>
#include "usersinfo.h"
#include <glib.h>

typedef enum pars_state {
    NAME,
    PASSWORD,
    OTHER
} ParserState;

typedef struct {
    UserInfo* cur_user;
    ParserState cur_state;
    UsersInfo* users;
} ParserContext;

void startElement(void *ctx, const xmlChar *fullname, const xmlChar **atts)
{
    ParserContext* pars_ctx = (ParserContext*) ctx;
    if (strcmp(fullname, "name") == 0) {
        pars_ctx->cur_state = NAME;
        return;
    }
    if (strcmp(fullname, "password") == 0) {
        pars_ctx->cur_state = PASSWORD;
        return;
    }
    if (strcmp(fullname, "user") == 0) {
        pars_ctx->cur_user = malloc(sizeof (*pars_ctx->cur_user));
    }
}

void characters(void *ctx, const xmlChar *ch, int len)
{
    printf("chars\n");
    ParserContext* pars_ctx = (ParserContext*) ctx;
    if (pars_ctx->cur_state == NAME) {
        printf("name:%.*s\n", len, ch);
        pars_ctx->cur_user->name = malloc((len+1)*sizeof(*(pars_ctx->cur_user->name)));
        memcpy(pars_ctx->cur_user->name, ch, len);
        pars_ctx->cur_user->name[len] = '\0';
        pars_ctx->cur_user->name_size = len;
        return;
    }
    if (pars_ctx->cur_state == PASSWORD) {
        pars_ctx->cur_user->password = malloc((len+1)*sizeof(*(pars_ctx->cur_user->password)));
        memcpy(pars_ctx->cur_user->password, ch, len);
        pars_ctx->cur_user->password[len] = '\0';
        pars_ctx->cur_user->passwd_size = len;
    }
}

void endElement(void *ctx, const xmlChar *name)
{
    ParserContext* pars_ctx = (ParserContext*) ctx;
    if (strcmp(name, "user") == 0) {
        g_hash_table_insert(pars_ctx->users, pars_ctx->cur_user->name, pars_ctx->cur_user);
    }
    pars_ctx->cur_state = OTHER;
}

xmlSAXHandler make_sax_handler (){
    xmlSAXHandler SAXHandler = {NULL};
    //SAXHander.initialized = XML_SAX2_MAGIC;
    SAXHandler.startElement = startElement;
    SAXHandler.characters = characters;
    SAXHandler.endElement = endElement;
    return SAXHandler;
} 

UsersInfo* parse_users_info(char* filename)
{
    xmlSAXHandler xml_handler = make_sax_handler();
    ParserContext pars_ctx = {NULL, OTHER, users_info_new()};
    if (xmlSAXUserParseFile(&xml_handler, &pars_ctx, filename) != 0){
        return NULL;
    }
    return pars_ctx.users;
}


