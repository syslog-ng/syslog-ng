#include "upgrade_log.h"
#include <libxml/HTMLtree.h>
#include <libxml/HTMLparser.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <unistd.h>

struct _UpgradeLog {
  xmlDoc *doc;
  xmlNode *root;
  xmlNode *head;
  xmlNode *body;
  gchar *filename;
};

void
upgrade_log_free(UpgradeLog *self)
{
  g_free(self->filename);
  xmlFreeDoc(self->doc);
  g_free(self);
}

static void
upgrade_log_parse_details(UpgradeLog *self, xmlNode *root, gchar *details)
{
  xmlNode *node;
  xmlNode *li;
  gchar *pos = details;
  gchar *start = pos;
  pos = strchr(start, ',');
  while (pos)
    {
      *pos = '\0';
      li =  xmlNewDocNode(self->doc, NULL, (xmlChar *)"li", NULL);
      node = xmlNewText((xmlChar *)start);
      xmlAddChild(li, node);
      xmlAddChild(root, li);
      *pos = ',';
      pos++;
      start = pos;
      pos = strchr(start, ',');
    }
  if (strlen(start) > 3)
    {
      li = xmlNewDocNode(self->doc, NULL, (xmlChar *)"li", NULL);
      node = xmlNewText((xmlChar *)start);
      xmlAddChild(li, node);
      xmlAddChild(root, li);
    }
}

static void
upgrade_log_parse_message(UpgradeLog *self, xmlNode *font, gchar *message)
{
  gchar *pos = message;
  gchar *start = message;
  xmlNode *node;
  pos = strchr(start, '\n');
  while (pos)
    {
      *pos = '\0';
      node = xmlNewText((xmlChar *)start);
      xmlAddChild(font, node);
      *pos = '\n';
      node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"br", NULL);
      xmlAddChild(font, node);
      pos++;
      start = pos;
      pos = strchr(start, '\n');
    }
  pos = strchr(start, ';');
  if (pos)
    {
      *pos = '\0';
      node = xmlNewText((xmlChar *)start);
      xmlAddChild(font, node);
      *pos = ';';
      node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"br", NULL);
      xmlAddChild(font, node);
      pos++;
      if (strlen(pos) > 0)
        {
          node = xmlNewText((xmlChar *)"Details:");
          xmlAddChild(font, node);
          node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"ul", NULL);
          upgrade_log_parse_details(self, node, pos);
          xmlAddChild(font, node);
        }
    }
  else
    {
      node = xmlNewText((xmlChar *)start);
      xmlAddChild(font, node);
    }
}


void
upgrade_log_error(UpgradeLog *self, gchar *msg_format, ...)
{
  gchar *message;
  va_list arg;
  va_start(arg, msg_format);
  message = g_strdup_vprintf(msg_format, arg);
  va_end(arg);
  xmlNode *h_node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"h3", NULL);
  xmlNode *node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"font", NULL);
  xmlSetProp(node, (xmlChar *)"color", (xmlChar *)"red");
  upgrade_log_parse_message(self, node, message);
  xmlAddChild(h_node, node);
  xmlAddChild(self->body, h_node);
  unlink(self->filename);
  htmlSaveFile(self->filename, self->doc);
  g_free(message);
}

void
upgrade_log_warning(UpgradeLog *self, gchar *msg_format, ...)
{
  gchar *message;
  va_list arg;
  va_start(arg, msg_format);
  message = g_strdup_vprintf(msg_format, arg);
  va_end(arg);
  xmlNode *h_node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"h3", NULL);
  xmlNode *node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"font", NULL);
  xmlSetProp(node, (xmlChar *)"color", (xmlChar *)"black");
  upgrade_log_parse_message(self, node, message);
  xmlAddChild(h_node, node);
  xmlAddChild(self->body, h_node);
  unlink(self->filename);
  htmlSaveFile(self->filename, self->doc);
  g_free(message);
}

void
upgrade_log_info(UpgradeLog *self, gchar *msg_format, ...)
{
  gchar *message;
  va_list arg;
  va_start(arg, msg_format);
  message = g_strdup_vprintf(msg_format, arg);
  va_end(arg);
  xmlNode *h_node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"h3", NULL);
  xmlNode *node = xmlNewDocNode(self->doc, NULL, (xmlChar *)"font", NULL);
  xmlSetProp(node, (xmlChar *)"color", (xmlChar *)"black");
  upgrade_log_parse_message(self, node, message);
  xmlAddChild(h_node, node);
  xmlAddChild(self->body, h_node);
  unlink(self->filename);
  htmlSaveFile(self->filename, self->doc);
  g_free(message);
}

static gchar *
get_time_stamp()
{
  gchar outstr[200];
  time_t t;
  struct tm *tmp;

  t = time(NULL);
  tmp = localtime(&t);
  if (tmp == NULL)
    {
      return NULL;
    }

  if (strftime(outstr, sizeof(outstr), " %Y-%m-%d %H:%M:%S", tmp) == 0)
    {
      return NULL;
    }
  return g_strdup(outstr);
}


UpgradeLog *
upgrade_log_new(gchar *filename)
{
  UpgradeLog *self = g_new0(UpgradeLog, 1);
  gchar *current_timestamp = get_time_stamp();
  self->filename = g_strdup(filename);
  if (g_file_test(self->filename, G_FILE_TEST_EXISTS))
    {
      gchar *file_content;
      gsize length;
      if (g_file_get_contents(self->filename, &file_content, &length, NULL))
        {
          self->doc = htmlParseDoc((xmlChar *)file_content, NULL);
          if (self->doc)
            {
              self->root = xmlDocGetRootElement(self->doc);
              self->head = self->root->children;
              self->body = self->head->next;
            }
        }
    }
  if (self->doc == NULL)
    {
      self->doc = htmlNewDoc(NULL, NULL);
      self->root = xmlNewDocNode(self->doc, NULL, (xmlChar *)"HTML", NULL);
      self->head = xmlNewDocNode(self->doc, NULL, (xmlChar *)"HEAD", NULL);
      self->body = xmlNewDocNode(self->doc, NULL, (xmlChar *)"BODY", NULL);
      xmlDocSetRootElement(self->doc, self->root);
      xmlAddChild(self->root, self->head);
      xmlAddChild(self->root, self->body);
      xmlAddChild(self->head, xmlNewDocNode(self->doc, NULL, (xmlChar *)"title", (xmlChar *)"syslog-ng Agent upgrade result"));
    }
  upgrade_log_info(self, "Starting upgrade: %s", current_timestamp);
  return self;
}
