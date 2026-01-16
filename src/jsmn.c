/*
 * Minimalistic JSON parser in C.
 *
 * Source: https://github.com/zserge/jsmn
 * License: MIT.
 */
#include "jsmn.h"

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
                                  jsmntok_t *tokens, unsigned int num_tokens) {
  jsmntok_t *tok;
  if (parser->toknext >= num_tokens) {
    return 0;
  }
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
  tok->type = JSMN_UNDEFINED;
  return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
                            int start, int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                unsigned int len, jsmntok_t *tokens,
                                unsigned int num_tokens) {
  jsmntok_t *token;
  int start = parser->pos;

  for (; parser->pos < len; parser->pos++) {
    switch (js[parser->pos]) {
      case '\t':
      case '\r':
      case '\n':
      case ' ':
      case ',':
      case ']':
      case '}':
        goto found;
      default:
        break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }

found:
  if (tokens == 0) {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == 0) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
  parser->pos--;
  return 0;
}

static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             unsigned int len, jsmntok_t *tokens,
                             unsigned int num_tokens) {
  jsmntok_t *token;
  int start = parser->pos;

  parser->pos++;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];

    if (c == '\"') {
      if (tokens == 0) {
        return 0;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == 0) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
      return 0;
    }

    if (c == '\\' && parser->pos + 1 < len) {
      parser->pos++;
      switch (js[parser->pos]) {
        case '\"':
        case '/':
        case '\\':
        case 'b':
        case 'f':
        case 'r':
        case 'n':
        case 't':
          break;
        case 'u':
          parser->pos += 4;
          break;
        default:
          parser->pos = start;
          return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

int jsmn_parse(jsmn_parser *parser, const char *js, unsigned int len,
               jsmntok_t *tokens, unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *token;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];
    switch (c) {
      case '{':
      case '[':
        if (tokens == 0) {
          break;
        }
        token = jsmn_alloc_token(parser, tokens, num_tokens);
        if (token == 0) {
          return JSMN_ERROR_NOMEM;
        }
        if (parser->toksuper != -1) {
          tokens[parser->toksuper].size++;
        }
        token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
        token->start = parser->pos;
        parser->toksuper = parser->toknext - 1;
        break;
      case '}':
      case ']':
        if (tokens == 0) {
          break;
        }
        for (i = parser->toknext - 1; i >= 0; i--) {
          token = &tokens[i];
          if (token->start != -1 && token->end == -1) {
            if ((token->type == JSMN_OBJECT && c == '}') ||
                (token->type == JSMN_ARRAY && c == ']')) {
              token->end = parser->pos + 1;
              parser->toksuper = -1;
              break;
            } else {
              return JSMN_ERROR_INVAL;
            }
          }
        }
        if (i == -1) {
          return JSMN_ERROR_INVAL;
        }
        for (; i >= 0; i--) {
          token = &tokens[i];
          if (token->start != -1 && token->end == -1) {
            parser->toksuper = i;
            break;
          }
        }
        break;
      case '\"':
        r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
        if (r < 0) {
          return r;
        }
        if (parser->toksuper != -1 && tokens != 0) {
          tokens[parser->toksuper].size++;
        }
        break;
      case '\t':
      case '\r':
      case '\n':
      case ' ':
      case ':':
      case ',':
        break;
      default:
        r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
        if (r < 0) {
          return r;
        }
        if (parser->toksuper != -1 && tokens != 0) {
          tokens[parser->toksuper].size++;
        }
        break;
    }
  }

  for (i = parser->toknext - 1; i >= 0; i--) {
    if (tokens[i].start != -1 && tokens[i].end == -1) {
      return JSMN_ERROR_PART;
    }
  }
  return parser->toknext;
}
