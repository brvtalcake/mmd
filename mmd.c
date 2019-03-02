/*
 * Implementation of miniature markdown library.
 *
 *     https://github.com/michaelrsweet/mmd
 *
 * Copyright © 2017-2019 by Michael R Sweet.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Define DEBUG to get debug printf messages to stderr.
 */

#define DEBUG 0
#if DEBUG
#  define DEBUG_printf(...)	fprintf(stderr, __VA_ARGS__)
#  define DEBUG_puts(s)		fputs(s, stderr);
#else
#  define DEBUG_printf(...)
#  define DEBUG_puts(s)
#endif /* DEBUG */


/*
 * Beginning with VC2005, Microsoft breaks ISO C and POSIX conformance
 * by deprecating a number of functions in the name of security, even
 * when many of the affected functions are otherwise completely secure.
 * The _CRT_SECURE_NO_DEPRECATE definition ensures that we won't get
 * warnings from their use...
 *
 * Then Microsoft decided that they should ignore this in VC2008 and use
 * yet another define (_CRT_SECURE_NO_WARNINGS) instead...
 */

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS


/*
 * Include necessary headers...
 */

#include "mmd.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


/*
 * Microsoft renames the POSIX functions to _name, and introduces a broken
 * compatibility layer using the original names.  As a result, random crashes
 * can occur when, for example, strdup() allocates memory from a different heap
 * than used by malloc() and free().
 *
 * To avoid moronic problems like this, we #define the POSIX function names to
 * the corresponding non-standard Microsoft names.
 */

#ifdef _WIN32
#  define snprintf 	_snprintf
#  define strdup	_strdup
#endif /* _WIN32 */


/*
 * Structures...
 */

struct _mmd_s
{
  mmd_type_t    type;                   /* Node type */
  int           whitespace;             /* Leading whitespace? */
  char          *text,                  /* Text */
                *url;                   /* Reference URL (image/link/etc.) */
  mmd_t         *parent,                /* Parent node */
                *first_child,           /* First child node */
                *last_child,            /* Last child node */
                *prev_sibling,          /* Previous sibling node */
                *next_sibling;          /* Next sibling node */
};

typedef struct _mmd_ref_s
{
  char		*name,			/* Name of reference */
		*url;			/* Reference URL */
  size_t	num_pending;		/* Number of pending nodes */
  mmd_t		**pending;		/* Pending nodes */
} _mmd_ref_t;

typedef struct _mmd_doc_s
{
  mmd_t		*root;			/* Root node */
  size_t	num_references;		/* Number of references */
  _mmd_ref_t	*references;		/* References */
} _mmd_doc_t;


/*
 * Local functions...
 */


static mmd_t    *mmd_add(mmd_t *parent, mmd_type_t type, int whitespace, char *text, char *url);
static void     mmd_free(mmd_t *node);
static int	mmd_is_table(FILE *fp);
static void     mmd_parse_inline(_mmd_doc_t *doc, mmd_t *parent, char *line);
static char     *mmd_parse_link(_mmd_doc_t *doc, char *lineptr, char **text, char **url, char **refname);
static void	mmd_ref_add(_mmd_doc_t *doc, mmd_t *node, const char *name, const char *url);
static _mmd_ref_t *mmd_ref_find(_mmd_doc_t *doc, const char *name);
static void     mmd_remove(mmd_t *node);
#if DEBUG
static const char *mmd_type_string(mmd_type_t type);
#endif /* DEBUG */


/*
 * 'mmdCopyAllText()' - Make a copy of all the text under a given node.
 *
 * The returned string must be freed using free().
 */

char *					/* O - Copied string */
mmdCopyAllText(mmd_t *node)		/* I - Parent node */
{
  char		*all = NULL,		/* String buffer */
		*allptr = NULL,		/* Pointer into string buffer */
		*temp;			/* Temporary pointer */
  size_t	allsize = 0,		/* Size of "all" buffer */
		textlen;		/* Length of "text" string */
  mmd_t		*current,		/* Current node */
		*next;			/* Next node */


  current = mmdGetFirstChild(node);

  while (current != node)
  {
    if (current->text)
    {
     /*
      * Append this node's text to the string...
      */

      textlen = strlen(current->text);

      if (allsize == 0)
      {
        allsize = textlen + (size_t)current->whitespace + 1;
        all     = malloc(allsize);
        allptr  = all;

	if (!all)
	  return (NULL);
      }
      else
      {
        allsize += textlen + (size_t)current->whitespace;
        temp    = realloc(all, allsize);

        if (!temp)
        {
          free(all);
          return (NULL);
        }

        allptr = temp + (allptr - all);
        all    = temp;
      }

      if (current->whitespace)
        *allptr++ = ' ';

      memcpy(allptr, current->text, textlen);
      allptr += textlen;
    }

   /*
    * Find the next logical node...
    */

    if ((next = mmdGetNextSibling(current)) == NULL)
    {
      next = mmdGetParent(current);

      while (next && next != node && mmdGetNextSibling(next) == NULL)
	next = mmdGetParent(next);

      if (next != node)
        next = mmdGetNextSibling(next);
    }

    current = next;
  }

  if (allptr)
    *allptr = '\0';

  return (all);
}


/*
 * 'mmdFree()' - Free a markdown tree.
 */

void
mmdFree(mmd_t *node)                    /* I - First node */
{
  mmd_t *current,		        /* Current node */
	*next;			        /* Next node */


  mmd_remove(node);

  for (current = node->first_child; current; current = next)
  {
   /*
    * Get the next node...
    */

    if ((next = current->first_child) != NULL)
    {
     /*
      * Free parent nodes after child nodes have been freed...
      */

      current->first_child = NULL;
      continue;
    }

    if ((next = current->next_sibling) == NULL)
    {
     /*
      * Next node is the parent, which we'll free as needed...
      */

      if ((next = current->parent) == node)
        next = NULL;
    }

   /*
    * Free child...
    */

    mmd_free(current);
  }

 /*
  * Then free the memory used by the parent node...
  */

  mmd_free(node);
}


/*
 * 'mmdGetFirstChild()' - Return the first child of a node, if any.
 */

mmd_t *                                 /* O - First child or @code NULL@ if none */
mmdGetFirstChild(mmd_t *node)           /* I - Node */
{
  return (node ? node->first_child : NULL);
}


/*
 * 'mmdGetLastChild()' - Return the last child of a node, if any.
 */

mmd_t *                                 /* O - Last child or @code NULL@ if none */
mmdGetLastChild(mmd_t *node)            /* I - Node */
{
  return (node ? node->last_child : NULL);
}


/*
 * 'mmdGetMetadata()' - Return the metadata for the given keyword.
 */

const char *                            /* O - Value or @code NULL@ if none */
mmdGetMetadata(mmd_t      *doc,         /* I - Document */
               const char *keyword)     /* I - Keyword */
{
  mmd_t         *metadata,              /* Metadata node */
                *current;               /* Current node */
  char          prefix[256];            /* Prefix string */
  size_t        prefix_len;             /* Length of prefix string */
  const char    *value;                 /* Pointer to value */


  if (!doc || (metadata = doc->first_child) == NULL || metadata->type != MMD_TYPE_METADATA)
    return (NULL);

  snprintf(prefix, sizeof(prefix), "%s:", keyword);
  prefix_len = strlen(prefix);

  for (current = metadata->first_child; current; current = current->next_sibling)
  {
    if (strncmp(current->text, prefix, prefix_len))
      continue;

    value = current->text + prefix_len;
    while (isspace(*value & 255))
      value ++;

    return (value);
  }

  return (NULL);
}


/*
 * 'mmdGetNextSibling()' - Return the next sibling of a node, if any.
 */

mmd_t *                                 /* O - Next sibling or @code NULL@ if none */
mmdGetNextSibling(mmd_t *node)          /* I - Node */
{
  return (node ? node->next_sibling : NULL);
}


/*
 * 'mmdGetParent()' - Return the parent of a node, if any.
 */

mmd_t *                                 /* O - Parent node or @code NULL@ if none */
mmdGetParent(mmd_t *node)               /* I - Node */
{
  return (node ? node->parent : NULL);
}


/*
 * 'mmdGetPrevSibling()' - Return the previous sibling of a node, if any.
 */

mmd_t *                                 /* O - Previous sibling or @code NULL@ if none */
mmdGetPrevSibling(mmd_t *node)          /* I - Node */
{
  return (node ? node->prev_sibling : NULL);
}


/*
 * 'mmdGetText()' - Return the text associated with a node, if any.
 */

const char *                            /* O - Text or @code NULL@ if none */
mmdGetText(mmd_t *node)                 /* I - Node */
{
  return (node ? node->text : NULL);
}


/*
 * 'mmdGetType()' - Return the type of a node, if any.
 */

mmd_type_t                              /* O - Type or @code MMD_TYPE_NONE@ if none */
mmdGetType(mmd_t *node)                 /* I - Node */
{
  return (node ? node->type : MMD_TYPE_NONE);
}


/*
 * 'mmdGetURL()' - Return the URL associated with a node, if any.
 */

const char *                            /* O - URL or @code NULL@ if none */
mmdGetURL(mmd_t *node)                  /* I - Node */
{
  return (node ? node->url : NULL);
}


/*
 * 'mmdGetWhitespace()' - Return whether whitespace preceded a node.
 */

int                                     /* O - 1 for whitespace, 0 for none */
mmdGetWhitespace(mmd_t *node)           /* I - Node */
{
  return (node ? node->whitespace : 0);
}


/*
 * 'mmdIsBlock()' - Return whether the node is a block.
 */

int                                     /* O - 1 for block nodes, 0 otherwise */
mmdIsBlock(mmd_t *node)                 /* I - Node */
{
  return (node ? node->type < MMD_TYPE_NORMAL_TEXT : 0);
}


/*
 * 'mmdLoad()' - Load a markdown file into nodes.
 */

mmd_t *                                 /* O - First node in markdown */
mmdLoad(const char *filename)           /* I - File to load */
{
  FILE          *fp;                    /* File */
  mmd_t         *doc;                   /* Document */


 /*
  * Open the file and create an empty document...
  */

  if ((fp = fopen(filename, "r")) == NULL)
    return (NULL);

  doc = mmdLoadFile(fp);

  fclose(fp);

  return (doc);
}


/*
 * 'mmdLoadFile()' - Load a markdown file into nodes from a stdio file.
 */

mmd_t *                                 /* O - First node in markdown */
mmdLoadFile(FILE *fp)                   /* I - File to load */
{
  size_t	i;			/* Looping var */
  _mmd_doc_t	doc;			/* Document */
  _mmd_ref_t	*reference;		/* Current reference */
  mmd_t         *current,               /* Current parent block */
                *block = NULL;          /* Current block */
  mmd_type_t    type;                   /* Type for line */
  char          line[65536],            /* Line from file */
                *lineptr,               /* Pointer into line */
                *lineend;               /* End of line */
  int           blank_code = 0;         /* Saved indented blank code line */
  mmd_type_t	columns[256];		/* Alignment of table columns */
  int		num_columns = 0,	/* Number of columns in table */
		rows = 0;		/* Number of rows in table */


 /*
  * Create an empty document...
  */

  memset(&doc, 0, sizeof(doc));

  doc.root = current = mmd_add(NULL, MMD_TYPE_DOCUMENT, 0, NULL, NULL);

  if (!doc.root)
  {
    fclose(fp);
    return (NULL);
  }

 /*
  * Read lines until end-of-file...
  */

  while (fgets(line, sizeof(line), fp))
  {
    lineptr = line;

    while (isspace(*lineptr & 255))
      lineptr ++;

    if ((lineptr - line) >= 4 && !block && (current == doc.root || current->type == MMD_TYPE_CODE_BLOCK))
    {
     /*
      * Indented code block.
      */

      if (current == doc.root)
        current = mmd_add(doc.root, MMD_TYPE_CODE_BLOCK, 0, NULL, NULL);

      if (blank_code)
        mmd_add(current, MMD_TYPE_CODE_TEXT, 0, "\n", NULL);

      mmd_add(current, MMD_TYPE_CODE_TEXT, 0, line + 4, NULL);

      blank_code = 0;
      continue;
    }
    else if (*lineptr == '`' && (!lineptr[1] || lineptr[1] == '`'))
    {
      if (block)
      {
        if (block->type == MMD_TYPE_CODE_BLOCK)
        {
          DEBUG_puts("Ending code block...\n");
          block = NULL;
        }
        else if (block->type == MMD_TYPE_LIST_ITEM)
          block = mmd_add(block, MMD_TYPE_CODE_BLOCK, 0, NULL, NULL);
        else if (block->parent->type == MMD_TYPE_LIST_ITEM)
          block = mmd_add(block->parent, MMD_TYPE_CODE_BLOCK, 0, NULL, NULL);
        else
          block = mmd_add(current, MMD_TYPE_CODE_BLOCK, 0, NULL, NULL);
      }
      else
        block = mmd_add(current, MMD_TYPE_CODE_BLOCK, 0, NULL, NULL);

      continue;
    }

    if (block && block->type == MMD_TYPE_CODE_BLOCK)
    {
      mmd_add(block, MMD_TYPE_CODE_TEXT, 0, line, NULL);
      continue;
    }
    else if (!strncmp(lineptr, "---", 3) && doc.root->first_child == NULL)
    {
     /*
      * Document metadata...
      */

      block = mmd_add(doc.root, MMD_TYPE_METADATA, 0, NULL, NULL);

      while (fgets(line, sizeof(line), fp))
      {
        lineptr = line;

        while (isspace(*lineptr & 255))
          lineptr ++;

        if (!strncmp(line, "---", 3) || !strncmp(line, "...", 3))
          break;

        lineend = lineptr + strlen(lineptr) - 1;
        if (lineend > lineptr && *lineend == '\n')
          *lineend = '\0';

        mmd_add(block, MMD_TYPE_METADATA_TEXT, 0, lineptr, NULL);
      }

      block = NULL;
      continue;
    }
    else if (!block && (!strncmp(lineptr, "---", 3) || !strncmp(lineptr, "***", 3) || !strncmp(lineptr, "___", 3)))
    {
      int ch = *lineptr;

      lineptr += 3;
      while (*lineptr && (*lineptr == ch || isspace(*lineptr & 255)))
        lineptr ++;

      if (!*lineptr)
      {
        block = NULL;
        mmd_add(current, MMD_TYPE_THEMATIC_BREAK, 0, NULL, NULL);
        continue;
      }
    }

    if (*lineptr == '>')
    {
     /*
      * Block quote.  See if the parent of the current node is already a block
      * quote...
      */

      mmd_t *node;			/* Current node */

      for (node = current; node != doc.root && node->type != MMD_TYPE_BLOCK_QUOTE; node = node->parent);

      if (node == doc.root || node->type != MMD_TYPE_BLOCK_QUOTE)
        current = mmd_add(doc.root, MMD_TYPE_BLOCK_QUOTE, 0, NULL, NULL);

     /*
      * Skip whitespace after the ">"...
      */

      lineptr ++;
      while (isspace(*lineptr & 255))
        lineptr ++;
    }
    else if (current->type == MMD_TYPE_BLOCK_QUOTE)
      current = current->parent;
    else if (current->type == MMD_TYPE_TABLE && current->parent && current->parent->type == MMD_TYPE_BLOCK_QUOTE)
      current = current->parent->parent;

    if (!*lineptr)
    {
      blank_code = current->type == MMD_TYPE_CODE_BLOCK;
      block      = NULL;
      continue;
    }
    else if (strchr(lineptr, '|') && (current->type == MMD_TYPE_TABLE || mmd_is_table(fp)))
    {
     /*
      * Table...
      */

      int	col;			/* Current column */
      char	*start,			/* Start of column/cell */
		*end;			/* End of column/cell */
      mmd_t	*row = NULL,		/* Current row */
		*cell;			/* Current cell */

      DEBUG_printf("TABLE current=%p (%d), rows=%d\n", current, current->type, rows);

      if (current->type != MMD_TYPE_TABLE)
      {
        if (current != doc.root && current->type != MMD_TYPE_BLOCK_QUOTE)
          current = current->parent;

        DEBUG_printf("ADDING NEW TABLE to %p (%d)\n", current, current->type);

        current = mmd_add(current, MMD_TYPE_TABLE, 0, NULL, NULL);
        block   = mmd_add(current, MMD_TYPE_TABLE_HEADER, 0, NULL, NULL);

        for (col = 0; col < (int)(sizeof(columns) / sizeof(columns[0])); col ++)
          columns[col] = MMD_TYPE_TABLE_BODY_CELL_LEFT;

        num_columns = 0;
        rows        = -1;
      }
      else if (rows > 0)
      {
        if (rows == 1)
          block = mmd_add(current, MMD_TYPE_TABLE_BODY, 0, NULL, NULL);
      }
      else
        block = NULL;

      if (block)
        row = mmd_add(block, MMD_TYPE_TABLE_ROW, 0, NULL, NULL);

      if (*lineptr == '|')
        lineptr ++;			/* Skip leading pipe */

      if ((end = lineptr + strlen(lineptr) - 1) > lineptr)
      {
        while ((*end == '\n' || *end == 'r') && end > lineptr)
          end --;

        if (end > lineptr && *end == '|')
	  *end = '\0';			/* Truncate trailing pipe */
      }

      for (col = 0; lineptr && *lineptr && col < (int)(sizeof(columns) / sizeof(columns[0])); col ++)
      {
       /*
        * Get the bounds of the current cell...
        */

        start = lineptr;
        if ((lineptr = strchr(lineptr + 1, '|')) != NULL)
          *lineptr++ = '\0';

        if (block)
        {
         /*
          * Add a cell to this row...
          */

          if (block->type == MMD_TYPE_TABLE_HEADER)
            cell = mmd_add(row, MMD_TYPE_TABLE_HEADER_CELL, 0, NULL, NULL);
          else
            cell = mmd_add(row, columns[col], 0, NULL, NULL);

          mmd_parse_inline(&doc, cell, start);
        }
        else
        {
         /*
          * Process separator row for alignment...
          */

	  while (isspace(*start & 255))
	    start ++;

          for (end = start + strlen(start) - 1; end > start && isspace(*end & 255); end --);

          if (*start == ':' && *end == ':')
            columns[col] = MMD_TYPE_TABLE_BODY_CELL_CENTER;
          else if (*end == ':')
            columns[col] = MMD_TYPE_TABLE_BODY_CELL_RIGHT;

          DEBUG_printf("COLUMN %d SEPARATOR=\"%s\", TYPE=%d\n", col, start, columns[col]);
        }
      }

     /*
      * Make sure the table is balanced...
      */

      if (col > num_columns)
      {
        num_columns = col;
      }
      else if (block && block->type != MMD_TYPE_TABLE_HEADER)
      {
        while (col < num_columns)
        {
          mmd_add(row, columns[col], 0, NULL, NULL);
          col ++;
        }
      }

      rows ++;
      continue;
    }
    else if (current->type == MMD_TYPE_TABLE)
    {
      DEBUG_puts("END TABLE\n");
      current = current->parent;
      block   = NULL;
    }

    if (!strcmp(lineptr, "+"))
    {
      if (block)
      {
        if (block->type == MMD_TYPE_LIST_ITEM)
          block = mmd_add(block, MMD_TYPE_PARAGRAPH, 0, NULL, NULL);
        else if (block->parent->type == MMD_TYPE_LIST_ITEM)
          block = mmd_add(block->parent, MMD_TYPE_PARAGRAPH, 0, NULL, NULL);
        else
          block = NULL;
      }
      continue;
    }
    else if (block && block->type == MMD_TYPE_PARAGRAPH && (!strncmp(lineptr, "---", 3) || !strncmp(lineptr, "===", 3)))
    {
      int ch = *lineptr;

      lineptr += 3;
      while (*lineptr == ch)
        lineptr ++;
      while (isspace(*lineptr & 255))
        lineptr ++;

      if (!*lineptr)
      {
        if (ch == '=')
          block->type = MMD_TYPE_HEADING_1;
        else
          block->type = MMD_TYPE_HEADING_2;

        block = NULL;
        continue;
      }

      type = MMD_TYPE_PARAGRAPH;
    }
    else if ((*lineptr == '-' || *lineptr == '+' || *lineptr == '*') && isspace(lineptr[1] & 255))
    {
     /*
      * Bulleted list...
      */

      lineptr += 2;
      while (isspace(*lineptr & 255))
        lineptr ++;

      if (current == doc.root && doc.root->last_child && doc.root->last_child->type == MMD_TYPE_UNORDERED_LIST)
        current = doc.root->last_child;
      else if (current->type != MMD_TYPE_UNORDERED_LIST)
        current = mmd_add(current->type == MMD_TYPE_BLOCK_QUOTE ? current : doc.root, MMD_TYPE_UNORDERED_LIST, 0, NULL, NULL);

      type  = MMD_TYPE_LIST_ITEM;
      block = NULL;
    }
    else if (isdigit(*lineptr & 255))
    {
     /*
      * Ordered list?
      */

      char *temp = lineptr + 1;

      while (isdigit(*temp & 255))
        temp ++;

      if (*temp == '.' && isspace(temp[1] & 255))
      {
       /*
        * Yes, ordered list.
        */

        lineptr = temp + 2;
        while (isspace(*lineptr & 255))
          lineptr ++;

        if (current == doc.root && doc.root->last_child && doc.root->last_child->type == MMD_TYPE_ORDERED_LIST)
          current = doc.root->last_child;
        else if (current->type != MMD_TYPE_ORDERED_LIST)
          current = mmd_add(current, MMD_TYPE_ORDERED_LIST, 0, NULL, NULL);

        type  = MMD_TYPE_LIST_ITEM;
        block = NULL;
      }
      else
      {
       /*
        * No, just a regular paragraph...
        */

        type = block ? block->type : MMD_TYPE_PARAGRAPH;
      }
    }
    else if (*lineptr == '#')
    {
     /*
      * Heading, count the number of '#' for the heading level...
      */

      char *temp = lineptr + 1;

      while (*temp == '#')
        temp ++;

      if ((temp - lineptr) <= 6)
      {
       /*
        * Heading 1-6...
        */

        type  = MMD_TYPE_HEADING_1 + (temp - lineptr - 1);
        block = NULL;

       /*
        * Skip whitespace after "#"...
        */

        lineptr = temp;
        while (isspace(*lineptr & 255))
          lineptr ++;

       /*
        * Strip trailing "#" characters...
        */

        for (temp = lineptr + strlen(lineptr) - 1; temp > lineptr && *temp == '#'; temp --)
          *temp = '\0';
      }
      else
      {
       /*
        * More than 6 #'s, just treat as a paragraph...
        */

        type = MMD_TYPE_PARAGRAPH;
      }

      if (current->type != MMD_TYPE_BLOCK_QUOTE)
        current = doc.root;
    }
    else if (!block)
    {
      type = MMD_TYPE_PARAGRAPH;

      if (lineptr == line)
        current = doc.root;
    }
    else
      type = block->type;

    if (!block || block->type != type)
    {
      if (current->type == MMD_TYPE_CODE_BLOCK)
        current = doc.root;

      block = mmd_add(current, type, 0, NULL, NULL);
    }

    mmd_parse_inline(&doc, block, lineptr);
  }

 /*
  * Free any references...
  */

  for (i = doc.num_references, reference = doc.references; i > 0; i --, reference ++)
  {
    if (reference->pending)
    {
      size_t	j;			/* Looping var */

      for (j = 0; j < reference->num_pending; j ++)
        reference->pending[j]->url = strdup(reference->name);

      free(reference->pending);
    }

    free(reference->name);
    free(reference->url);
  }

  free(doc.references);

 /*
  * Return the root node...
  */

  return (doc.root);
}


/*
 * 'mmd_add()' - Add a new markdown node.
 */

static mmd_t *                          /* O - New node */
mmd_add(mmd_t      *parent,             /* I - Parent node */
        mmd_type_t type,                /* I - Node type */
        int        whitespace,          /* I - 1 if whitespace precedes this node */
        char       *text,               /* I - Text, if any */
        char       *url)                /* I - URL, if any */
{
  mmd_t         *temp;                  /* New node */


  DEBUG_printf("Adding %s to %p(%s), whitespace=%d, text=\"%s\", url=\"%s\"\n", mmd_type_string(type), parent, parent ? mmd_type_string(parent->type) : "", whitespace, text ? text : "(null)", url ? url : "(null)");

  if ((temp = calloc(1, sizeof(mmd_t))) != NULL)
  {
    if (parent)
    {
     /*
      * Add node to the parent...
      */

      temp->parent = parent;

      if (parent->last_child)
      {
        parent->last_child->next_sibling = temp;
        temp->prev_sibling               = parent->last_child;
        parent->last_child               = temp;
      }
      else
      {
        parent->first_child = parent->last_child = temp;
      }
    }

   /*
    * Copy the node values...
    */

    temp->type       = type;
    temp->whitespace = whitespace;

    if (text)
      temp->text = strdup(text);

    if (url)
      temp->url = strdup(url);
  }

  return (temp);
}


/*
 * 'mmd_free()' - Free memory used by a node.
 */

static void
mmd_free(mmd_t *node)                   /* I - Node */
{
  if (node->text)
    free(node->text);

  if (node->url)
    free(node->url);

  free(node);
}


/*
 * 'mmd_is_table()' - Look ahead to see if the next line contains a heading
 *                    divider for a table.
 */

static int				/* O - 1 if this is a table, 0 otherwise */
mmd_is_table(FILE *fp)			/* I - File to read from */
{
  int	is_table = 0;			/* Is this a table? */
  long	pos;				/* Current position in file */
  char	line[65536],			/* Line from file */
	*ptr;				/* Pointer into line */


  pos = ftell(fp);

  if (fgets(line, sizeof(line), fp))
  {
    for (ptr = line; *ptr; ptr ++)
    {
      if (*ptr == '>' && ptr == line)
	continue;
      else if (!strchr(" \t\n\r:-|", *ptr))
	break;
    }

    is_table = !*ptr;
  }

  fseek(fp, pos, SEEK_SET);

  return (is_table);
}


/*
 * 'mmd_parse_inline()' - Parse inline formatting.
 */

static void
mmd_parse_inline(_mmd_doc_t *doc,	/* I - Document */
                 mmd_t      *parent,	/* I - Parent node */
                 char       *line)	/* I - Line from file */
{
  mmd_t		*node;			/* New node */
  mmd_type_t    type;                   /* Current node type */
  int           whitespace;             /* Whitespace precedes? */
  char          *lineptr,               /* Pointer into line */
                *text,                  /* Text fragment in line */
                *url,                   /* URL in link */
                *refname;		/* Reference name */


  whitespace = parent->last_child != NULL;

  for (lineptr = line, text = NULL, type = MMD_TYPE_NORMAL_TEXT; *lineptr; lineptr ++)
  {
    if (isspace(*lineptr & 255) && type != MMD_TYPE_CODE_TEXT)
    {
      if (text)
      {
        *lineptr = '\0';
        mmd_add(parent, type, whitespace, text, NULL);
        text = NULL;
      }

      whitespace = 1;

      if (isspace(lineptr[1] & 255) && !lineptr[2])
        mmd_add(parent, MMD_TYPE_HARD_BREAK, 0, NULL, NULL);
    }
    else if (*lineptr == '!' && lineptr[1] == '[' && type != MMD_TYPE_CODE_TEXT)
    {
     /*
      * Image...
      */

      if (text)
      {
        mmd_add(parent, type, whitespace, text, NULL);

        text       = NULL;
        whitespace = 0;
      }

      lineptr = mmd_parse_link(doc, lineptr + 1, &text, &url, &refname);

      if (url || refname)
      {
        node = mmd_add(parent, MMD_TYPE_IMAGE, whitespace, text, url);

        if (refname)
          mmd_ref_add(doc, node, refname, NULL);
      }

      if (!*lineptr)
        return;

      text = url = NULL;
      whitespace = 0;
      lineptr --;
    }
    else if (*lineptr == '[' && type != MMD_TYPE_CODE_TEXT)
    {
     /*
      * Link...
      */

      if (text)
      {
        mmd_add(parent, type, whitespace, text, NULL);

        text       = NULL;
        whitespace = 0;
      }

      lineptr = mmd_parse_link(doc, lineptr, &text, &url, &refname);

      if (text && *text == '`')
      {
        char *end = text + strlen(text) - 1;

        text ++;
        if (end > text && *end == '`')
          *end = '\0';

        node = mmd_add(parent, MMD_TYPE_CODE_TEXT, whitespace, text, url);
      }
      else if (text)
        node = mmd_add(parent, MMD_TYPE_LINKED_TEXT, whitespace, text, url);
      else
        node = NULL;

      if (refname && node)
        mmd_ref_add(doc, node, refname, NULL);

      if (!*lineptr)
        return;

      text = url = NULL;
      whitespace = 0;
      lineptr --;
    }
    else if (*lineptr == '<' && type != MMD_TYPE_CODE_TEXT && strchr(lineptr + 1, '>'))
    {
     /*
      * Autolink...
      */

      if (text)
      {
        mmd_add(parent, type, whitespace, text, NULL);

        text       = NULL;
        whitespace = 0;
      }

      url     = lineptr + 1;
      lineptr = strchr(lineptr + 1, '>');

      *lineptr++ = '\0';

      mmd_add(parent, MMD_TYPE_LINKED_TEXT, whitespace, url, url);

      text = url = NULL;
      whitespace = 0;
      lineptr --;
    }
    else if ((*lineptr == '*' || *lineptr == '_') && type != MMD_TYPE_CODE_TEXT)
    {
      int delim = *lineptr;		/* Delimiter */

      if (text)
      {
        *lineptr = '\0';

        mmd_add(parent, type, whitespace, text, NULL);

        *lineptr   = (char )delim;
        text       = NULL;
        whitespace = 0;
      }

      if (type == MMD_TYPE_NORMAL_TEXT)
      {
        if (lineptr[1] == delim && !isspace(lineptr[2] & 255))
        {
          type = MMD_TYPE_STRONG_TEXT;
          lineptr ++;
        }
        else if (!isspace(lineptr[1] & 255))
        {
          type = MMD_TYPE_EMPHASIZED_TEXT;
        }

        text = lineptr + 1;
      }
      else
      {
        if (lineptr[1] == delim)
          lineptr ++;

        type = MMD_TYPE_NORMAL_TEXT;
      }
    }
    else if (lineptr[0] == '~' && lineptr[1] == '~' && type != MMD_TYPE_CODE_TEXT)
    {
      if (text)
      {
        *lineptr = '\0';

        mmd_add(parent, type, whitespace, text, NULL);

        *lineptr   = '~';
        text       = NULL;
        whitespace = 0;
      }

      if (!isspace(lineptr[2] & 255) && type == MMD_TYPE_NORMAL_TEXT)
      {
	type = MMD_TYPE_STRUCK_TEXT;
        text = lineptr + 2;
      }
      else
      {
	lineptr ++;
        type = MMD_TYPE_NORMAL_TEXT;
      }
    }
    else if (*lineptr == '`')
    {
      if (text)
      {
        *lineptr = '\0';
        mmd_add(parent, type, whitespace, text, NULL);

        text       = NULL;
        whitespace = 0;
      }

      if (type == MMD_TYPE_CODE_TEXT)
      {
        type = MMD_TYPE_NORMAL_TEXT;
      }
      else
      {
        type = MMD_TYPE_CODE_TEXT;
        text = lineptr + 1;
      }
    }
    else if (!text)
    {
      if (*lineptr == '\\' && lineptr[1])
      {
       /*
        * Escaped character...
        */

        lineptr ++;
      }

      text = lineptr;
    }
    else if (*lineptr == '\\' && lineptr[1])
    {
     /*
      * Escaped character...
      */

      memmove(lineptr, lineptr + 1, strlen(lineptr));
    }
  }

  if (text)
    mmd_add(parent, type, whitespace, text, NULL);
}


/*
 * 'mmd_parse_link()' - Parse a link.
 */

static char *				/* O - End of link text */
mmd_parse_link(_mmd_doc_t *doc,		/* I - Document */
               char       *lineptr,	/* I - Pointer into line */
               char       **text,	/* O - Text */
               char       **url,	/* O - URL */
               char       **refname)	/* O - Reference name */
{
  lineptr ++; /* skip "[" */

  *text    = lineptr;
  *url     = NULL;
  *refname = NULL;

  while (*lineptr && *lineptr != ']')
  {
    if (*lineptr == '\"')
    {
      lineptr ++;
      while (*lineptr && *lineptr != '\"')
        lineptr ++;

      if (!*lineptr)
        return (lineptr);
    }

    lineptr ++;
  }

  if (!*lineptr)
    return (lineptr);

  *lineptr++ = '\0';

  while (isspace(*lineptr & 255))
    lineptr ++;

  if (*lineptr == '(')
  {
   /*
    * Get URL...
    */

    lineptr ++;
    *url = lineptr;

    while (*lineptr && *lineptr != ')')
    {
      if (isspace(*lineptr & 255))
        *lineptr = '\0';
      else if (*lineptr == '\"')
      {
        lineptr ++;
        while (*lineptr && *lineptr != '\"')
          lineptr ++;

        if (!*lineptr)
          return (lineptr);
      }

      lineptr ++;
    }

    *lineptr++ = '\0';
  }
  else if (*lineptr == '[')
  {
   /*
    * Get reference...
    */

    lineptr ++;
    *refname = lineptr;

    while (*lineptr && *lineptr != ']')
    {
      if (isspace(*lineptr & 255))
        *lineptr = '\0';
      else if (*lineptr == '\"')
      {
        lineptr ++;
        while (*lineptr && *lineptr != '\"')
          lineptr ++;

        if (!*lineptr)
          return (lineptr);
      }

      lineptr ++;
    }

    *lineptr++ = '\0';
    if (!**refname)
      *refname = *text;
  }
  else if (*lineptr == ':')
  {
   /*
    * Get reference definition...
    */

    lineptr ++;
    while (*lineptr && isspace(*lineptr & 255))
      lineptr ++;

    *url = lineptr;

    while (*lineptr && !isspace(*lineptr & 255))
      lineptr ++;

    *lineptr = '\0';

    mmd_ref_add(doc, NULL, *text, *url);

    *text = NULL;
    *url  = NULL;
  }

  return (lineptr);
}


/*
 * 'mmd_ref_add()' - Add or update a reference...
 */

static void
mmd_ref_add(_mmd_doc_t *doc,		/* I - Document */
            mmd_t      *node,		/* I - Link node, if any */
            const char *name,		/* I - Reference name */
            const char *url)		/* I - Reference URL */
{
  size_t	i;			/* Looping var */
  _mmd_ref_t	*ref = mmd_ref_find(doc, name);
					/* Reference */


  if (ref)
  {
    if (!ref->url && url)
    {
      if (node)
        node->url = strdup(url);

      ref->url = strdup(url);

      for (i = 0; i < ref->num_pending; i ++)
        ref->pending[i]->url = strdup(url);

      free(ref->pending);

      ref->num_pending = 0;
      ref->pending     = NULL;
      return;
    }
  }
  else if ((ref = realloc(doc->references, (doc->num_references + 1) * sizeof(_mmd_ref_t))) != NULL)
  {
    doc->references = ref;
    ref += doc->num_references;
    doc->num_references ++;

    ref->name        = strdup(name);
    ref->url         = url ? strdup(url) : NULL;
    ref->num_pending = 0;
    ref->pending     = NULL;
  }
  else
    return;

  if (node)
  {
    if (ref->url)
      node->url = strdup(ref->url);
    else if ((ref->pending = realloc(ref->pending, (ref->num_pending + 1) * sizeof(mmd_t *))) != NULL)
      ref->pending[ref->num_pending ++] = node;
  }
}


/*
 * 'mmd_ref_find()' - Find a reference...
 */

static _mmd_ref_t *			/* O - Reference or NULL */
mmd_ref_find(_mmd_doc_t *doc,		/* I - Document */
             const char *name)		/* I - Reference name */
{
  size_t	i;			/* Looping var */


  for (i = 0; i < doc->num_references; i ++)
    if (!strcasecmp(name, doc->references[i].name))
      return (doc->references + i);

  return (NULL);
}


/*
 * 'mmd_remove()' - Remove a node from its parent.
 */

static void
mmd_remove(mmd_t *node)                 /* I - Node */
{
  if (node->parent)
  {
    if (node->prev_sibling)
      node->prev_sibling->next_sibling = node->next_sibling;
    else
      node->parent->first_child = node->next_sibling;

    if (node->next_sibling)
      node->next_sibling->prev_sibling = node->prev_sibling;
    else
      node->parent->last_child = node->prev_sibling;

    node->parent       = NULL;
    node->prev_sibling = NULL;
    node->next_sibling = NULL;
  }
}


#if DEBUG
/*
 * 'mmd_type_string()' - Return a string for the specified type enumeration.
 */

static const char *			/* O - String representing the type */
mmd_type_string(mmd_type_t type)	/* I - Type value */
{
  static char	unknown[64];		/* Unknown type buffer */


  switch (type)
  {
    case MMD_TYPE_NONE :
        return ("MMD_TYPE_NONE");
    case MMD_TYPE_DOCUMENT :
        return "MMD_TYPE_DOCUMENT";
    case MMD_TYPE_METADATA :
        return "MMD_TYPE_METADATA";
    case MMD_TYPE_BLOCK_QUOTE :
        return "MMD_TYPE_BLOCK_QUOTE";
    case MMD_TYPE_ORDERED_LIST :
        return "MMD_TYPE_ORDERED_LIST";
    case MMD_TYPE_UNORDERED_LIST :
        return "MMD_TYPE_UNORDERED_LIST";
    case MMD_TYPE_LIST_ITEM :
        return "MMD_TYPE_LIST_ITEM";
    case MMD_TYPE_TABLE :
        return "MMD_TYPE_TABLE";
    case MMD_TYPE_TABLE_HEADER :
        return "MMD_TYPE_TABLE_HEADER";
    case MMD_TYPE_TABLE_BODY :
        return "MMD_TYPE_TABLE_BODY";
    case MMD_TYPE_TABLE_ROW :
        return "MMD_TYPE_TABLE_ROW";
    case MMD_TYPE_HEADING_1 :
        return "MMD_TYPE_HEADING_1";
    case MMD_TYPE_HEADING_2 :
        return "MMD_TYPE_HEADING_2";
    case MMD_TYPE_HEADING_3 :
        return "MMD_TYPE_HEADING_3";
    case MMD_TYPE_HEADING_4 :
        return "MMD_TYPE_HEADING_4";
    case MMD_TYPE_HEADING_5 :
        return "MMD_TYPE_HEADING_5";
    case MMD_TYPE_HEADING_6 :
        return "MMD_TYPE_HEADING_6";
    case MMD_TYPE_PARAGRAPH :
        return "MMD_TYPE_PARAGRAPH";
    case MMD_TYPE_CODE_BLOCK :
        return "MMD_TYPE_CODE_BLOCK";
    case MMD_TYPE_THEMATIC_BREAK :
        return "MMD_TYPE_THEMATIC_BREAK";
    case MMD_TYPE_TABLE_HEADER_CELL :
        return "MMD_TYPE_TABLE_HEADER_CELL";
    case MMD_TYPE_TABLE_BODY_CELL_LEFT :
        return "MMD_TYPE_TABLE_BODY_CELL_LEFT";
    case MMD_TYPE_TABLE_BODY_CELL_CENTER :
        return "MMD_TYPE_TABLE_BODY_CELL_CENTER";
    case MMD_TYPE_TABLE_BODY_CELL_RIGHT :
        return "MMD_TYPE_TABLE_BODY_CELL_RIGHT";
    case MMD_TYPE_NORMAL_TEXT :
        return "MMD_TYPE_NORMAL_TEXT";
    case MMD_TYPE_EMPHASIZED_TEXT :
        return "MMD_TYPE_EMPHASIZED_TEXT";
    case MMD_TYPE_STRONG_TEXT :
        return "MMD_TYPE_STRONG_TEXT";
    case MMD_TYPE_STRUCK_TEXT :
        return "MMD_TYPE_STRUCK_TEXT";
    case MMD_TYPE_LINKED_TEXT :
        return "MMD_TYPE_LINKED_TEXT";
    case MMD_TYPE_CODE_TEXT :
        return "MMD_TYPE_CODE_TEXT";
    case MMD_TYPE_IMAGE :
        return "MMD_TYPE_IMAGE";
    case MMD_TYPE_HARD_BREAK :
        return "MMD_TYPE_HARD_BREAK";
    case MMD_TYPE_SOFT_BREAK :
        return "MMD_TYPE_SOFT_BREAK";
    case MMD_TYPE_METADATA_TEXT :
        return "MMD_TYPE_METADATA_TEXT";
    default :
        snprintf(unknown, sizeof(unknown), "?? %d ??", (int)type);
        return (unknown);
  }
}
#endif /* DEBUG */
