// SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Erlang C node that runs a godot-sandbox guest ELF inside libgodot.
// Request {Command, Ref, Arg}; reply {Ref, {ok, Term}} or {Ref, {error, Binary}}.
//   load   <<ELF>>              load the guest into a Sandbox
//   vmcall {<<"fn">>, [Arg..]}  call a guest function; Arg is integer, float, boolean or binary
//   ping   _                    reply {ok, pong}
#include <ei.h>
#include <ei_connect.h>
#include <gdextension_interface.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define MAX_ARGS 16

typedef GDExtensionObjectPtr (*create_fn)(int, char **, GDExtensionInitializationFunction);

static GDExtensionInterfaceGetProcAddress get_proc;
static GDExtensionInterfaceVariantCall variant_call;
static GDExtensionInterfaceGlobalGetSingleton global_singleton;
static GDExtensionInterfaceVariantDestroy variant_destroy;
static GDExtensionInterfaceVariantGetType variant_get_type;
static GDExtensionInterfaceVariantStringify variant_stringify;
static GDExtensionInterfaceStringToUtf8Chars string_to_utf8;
static GDExtensionInterfaceStringNewWithUtf8Chars string_new;
static GDExtensionInterfaceStringNewWithUtf8CharsAndLen2 string_new_len;
static GDExtensionInterfaceStringNameNewWithLatin1Chars name_new;
static GDExtensionInterfaceClassdbConstructObject2 construct_object;
static GDExtensionInterfacePackedByteArrayOperatorIndex pba_index;
static GDExtensionInterfaceVariantGetPtrConstructor ptr_constructor;
static GDExtensionInterfaceGetVariantFromTypeConstructor from_type;
static GDExtensionInterfaceGetVariantToTypeConstructor to_type;
static GDExtensionVariantFromTypeConstructorFunc from_int, from_float, from_bool, from_object, from_pba, from_string;
static GDExtensionTypeFromVariantConstructorFunc to_int, to_float, to_bool;

static void on_level(void *userdata, GDExtensionInitializationLevel level) { (void)userdata; (void)level; }

static GDExtensionBool init_ext(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr lib,
                                GDExtensionInitialization *init) {
  (void)lib;
  get_proc = proc;
  init->minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE;
  init->initialize = on_level;
  init->deinitialize = on_level;
  return 1;
}

#define BIND(var, name) var = (void *)get_proc(name)
static void bind_interface(void) {
  BIND(variant_call, "variant_call");
  BIND(variant_destroy, "variant_destroy");
  BIND(global_singleton, "global_get_singleton");
  BIND(variant_get_type, "variant_get_type");
  BIND(variant_stringify, "variant_stringify");
  BIND(string_to_utf8, "string_to_utf8_chars");
  BIND(string_new, "string_new_with_utf8_chars");
  BIND(string_new_len, "string_new_with_utf8_chars_and_len2");
  BIND(name_new, "string_name_new_with_latin1_chars");
  BIND(construct_object, "classdb_construct_object2");
  BIND(pba_index, "packed_byte_array_operator_index");
  BIND(ptr_constructor, "variant_get_ptr_constructor");
  BIND(from_type, "get_variant_from_type_constructor");
  BIND(to_type, "get_variant_to_type_constructor");
  from_int = from_type(GDEXTENSION_VARIANT_TYPE_INT);
  from_float = from_type(GDEXTENSION_VARIANT_TYPE_FLOAT);
  from_bool = from_type(GDEXTENSION_VARIANT_TYPE_BOOL);
  from_object = from_type(GDEXTENSION_VARIANT_TYPE_OBJECT);
  from_pba = from_type(GDEXTENSION_VARIANT_TYPE_PACKED_BYTE_ARRAY);
  from_string = from_type(GDEXTENSION_VARIANT_TYPE_STRING);
  to_int = to_type(GDEXTENSION_VARIANT_TYPE_INT);
  to_float = to_type(GDEXTENSION_VARIANT_TYPE_FLOAT);
  to_bool = to_type(GDEXTENSION_VARIANT_TYPE_BOOL);
}

typedef struct { _Alignas(16) uint8_t bytes[40]; } Var;  // a Variant is 24 or 40 bytes; 40 is safe
typedef struct { _Alignas(16) uint8_t bytes[16]; } Str;  // String, StringName, PackedByteArray

static GDExtensionObjectPtr instance, sandbox;

static int call_method(GDExtensionVariantPtr self, const char *method, Var *args, int argc, Var *ret, char *err, size_t errsize) {
  Str name;
  const GDExtensionConstVariantPtr *pointers = NULL;
  GDExtensionConstVariantPtr list[MAX_ARGS];
  GDExtensionCallError call_error = {0};
  for (int i = 0; i < argc; i++) list[i] = &args[i];
  if (argc) pointers = list;
  name_new(&name, method, 0);
  variant_call(self, &name, pointers, argc, ret, &call_error);
  if (call_error.error != GDEXTENSION_CALL_OK) {
    snprintf(err, errsize, "call %s failed: error %d argument %d", method, (int)call_error.error, (int)call_error.argument);
    return -1;
  }
  return 0;
}

static void object_variant(GDExtensionObjectPtr object, Var *out) {
  GDExtensionObjectPtr slot = object;
  from_object(out, &slot);
}

static void console_log(const char *text, const char *level) {
  Str cls; Var self, args[2], ret; char err[128]; Str a, b;
  name_new(&cls, "Console", 0);
  GDExtensionObjectPtr console = global_singleton(&cls);
  if (!console) return;
  string_new(&a, text); from_string(&args[0], &a);
  string_new(&b, level); from_string(&args[1], &b);
  object_variant(console, &self);
  if (call_method(&self, "log_line", args, 2, &ret, err, sizeof err) == 0) variant_destroy(&ret);
  variant_destroy(&args[0]); variant_destroy(&args[1]);
}

static int load_elf(const char *data, size_t size, char *err, size_t errsize) {
  Str array;
  Var arg, ret, self;
  GDExtensionPtrConstructor make = ptr_constructor(GDEXTENSION_VARIANT_TYPE_PACKED_BYTE_ARRAY, 0);
  Str sizearg;
  Var resize_arg, resize_ret;
  if (!sandbox) {
    Str cls;
    name_new(&cls, "Sandbox", 0);
    sandbox = construct_object(&cls);
    if (!sandbox) { snprintf(err, errsize, "cannot construct Sandbox: is the godot_sandbox extension loaded?"); return -1; }
  }
  make(&array, NULL);
  from_pba(&arg, &array);
  int64_t count = (int64_t)size;
  from_int(&resize_arg, &count);
  (void)sizearg;
  if (call_method(&arg, "resize", &resize_arg, 1, &resize_ret, err, errsize)) return -1;
  // The Variant holds its own copy of the array, so write through the variant's array.
  GDExtensionTypeFromVariantConstructorFunc to_pba = to_type(GDEXTENSION_VARIANT_TYPE_PACKED_BYTE_ARRAY);
  Str held;
  to_pba(&held, &arg);
  memcpy(pba_index(&held, 0), data, size);
  object_variant(sandbox, &self);
  if (call_method(&self, "load_buffer", &arg, 1, &ret, err, errsize)) return -1;
  variant_destroy(&ret);
  variant_destroy(&arg);
  { char line[96]; snprintf(line, sizeof line, "guest loaded, %zu bytes", size); console_log(line, "ok"); }
  return 0;
}

static void encode_variant(ei_x_buff *out, Var *value) {
  GDExtensionVariantType type = variant_get_type(value);
  if (type == GDEXTENSION_VARIANT_TYPE_INT) {
    int64_t n; to_int(&n, value); ei_x_encode_longlong(out, n);
  } else if (type == GDEXTENSION_VARIANT_TYPE_FLOAT) {
    double d; to_float(&d, value); ei_x_encode_double(out, d);
  } else if (type == GDEXTENSION_VARIANT_TYPE_BOOL) {
    uint8_t b; to_bool(&b, value); ei_x_encode_boolean(out, b);
  } else if (type == GDEXTENSION_VARIANT_TYPE_NIL) {
    ei_x_encode_atom(out, "nil");
  } else {
    Str text; char *buffer;
    variant_stringify(value, &text);
    GDExtensionInt length = string_to_utf8(&text, NULL, 0);
    buffer = malloc((size_t)length + 1);
    string_to_utf8(&text, buffer, length);
    ei_x_encode_binary(out, buffer, length);
    free(buffer);
  }
}

static int decode_args(const char *buff, int *index, int count, Var *args, char *err, size_t errsize) {
  for (int i = 0; i < count; i++) {
    int type, size;
    ei_get_type(buff, index, &type, &size);
    if (type == ERL_SMALL_INTEGER_EXT || type == ERL_INTEGER_EXT || type == ERL_SMALL_BIG_EXT) {
      long long n; ei_decode_longlong(buff, index, &n); int64_t v = n; from_int(&args[i], &v);
    } else if (type == ERL_FLOAT_EXT || type == NEW_FLOAT_EXT) {
      double d; ei_decode_double(buff, index, &d); from_float(&args[i], &d);
    } else if (type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_UTF8_EXT || type == ERL_ATOM_UTF8_EXT) {
      int b; if (ei_decode_boolean(buff, index, &b)) { snprintf(err, errsize, "argument %d: only true and false atoms", i); return -1; }
      uint8_t v = (uint8_t)b; from_bool(&args[i], &v);
    } else if (type == ERL_BINARY_EXT) {
      char *text = malloc((size_t)size + 1); long length;
      ei_decode_binary(buff, index, text, &length);
      Str s; string_new_len(&s, text, length); from_string(&args[i], &s); free(text);
    } else { snprintf(err, errsize, "argument %d: unsupported term type %d", i, type); return -1; }
  }
  return 0;
}

static void reply(int fd, const erlang_msg *message, const char *ref, int ref_size, const char *tag, const char *text, ei_x_buff *term) {
  ei_x_buff out;
  ei_x_new_with_version(&out);
  ei_x_encode_tuple_header(&out, 2);
  ei_x_append_buf(&out, ref, ref_size);
  ei_x_encode_tuple_header(&out, 2);
  ei_x_encode_atom(&out, tag);
  if (term) ei_x_append_buf(&out, term->buff, term->index);
  else ei_x_encode_binary(&out, text, (long)strlen(text));
  ei_send(fd, (erlang_pid *)&message->from, out.buff, out.index);
  ei_x_free(&out);
}

static void handle(int fd, const erlang_msg *message, ei_x_buff *request) {
  int version, index = 0, arity, start;
  char command[MAXATOMLEN], err[256] = "";
  if (ei_decode_version(request->buff, &index, &version) || ei_decode_tuple_header(request->buff, &index, &arity) || arity != 3 ||
      ei_decode_atom(request->buff, &index, command)) return;
  start = index;
  ei_skip_term(request->buff, &index);
  int ref_size = index - start;
  char *ref = malloc((size_t)ref_size);
  memcpy(ref, request->buff + start, (size_t)ref_size);
  if (!strcmp(command, "ping")) {
    ei_x_buff term; ei_x_new(&term); ei_x_encode_atom(&term, "pong");
    reply(fd, message, ref, ref_size, "ok", NULL, &term); ei_x_free(&term);
  } else if (!strcmp(command, "info")) {
    ei_x_buff term; ei_x_new(&term);
    ei_x_encode_map_header(&term, 4);
    ei_x_encode_atom(&term, "name"); ei_x_encode_binary(&term, "guest-cnode", 11);
    ei_x_encode_atom(&term, "version"); ei_x_encode_binary(&term, "0.1.0", 5);
    ei_x_encode_atom(&term, "status"); ei_x_encode_atom(&term, sandbox ? "loaded" : "empty");
    ei_x_encode_atom(&term, "commands");
    ei_x_encode_list_header(&term, 5);
    ei_x_encode_atom(&term, "ping"); ei_x_encode_atom(&term, "info"); ei_x_encode_atom(&term, "load");
    ei_x_encode_atom(&term, "vmcall"); ei_x_encode_atom(&term, "cancel");
    ei_x_encode_empty_list(&term);
    reply(fd, message, ref, ref_size, "ok", NULL, &term); ei_x_free(&term);
  } else if (!strcmp(command, "cancel")) {
    reply(fd, message, ref, ref_size, "error", "not cancellable", NULL);
  } else if (!strcmp(command, "load")) {
    int type, size; long length = 0;
    ei_get_type(request->buff, &index, &type, &size);
    char *data = type == ERL_BINARY_EXT ? malloc((size_t)size) : NULL;
    if (!data || ei_decode_binary(request->buff, &index, data, &length)) reply(fd, message, ref, ref_size, "error", "load needs a binary", NULL);
    else if (load_elf(data, (size_t)length, err, sizeof err)) reply(fd, message, ref, ref_size, "error", err, NULL);
    else { ei_x_buff term; ei_x_new(&term); ei_x_encode_atom(&term, "loaded"); reply(fd, message, ref, ref_size, "ok", NULL, &term); ei_x_free(&term); }
    free(data);
  } else if (!strcmp(command, "vmcall")) {
    int type, size, count = 0; long length;
    Var args[MAX_ARGS + 1], ret, self;
    if (!sandbox) { reply(fd, message, ref, ref_size, "error", "no guest loaded", NULL); free(ref); return; }
    if (ei_decode_tuple_header(request->buff, &index, &arity) || arity != 2) { reply(fd, message, ref, ref_size, "error", "vmcall needs {Fn, Args}", NULL); free(ref); return; }
    ei_get_type(request->buff, &index, &type, &size);
    char *fn = malloc((size_t)size + 1);
    ei_decode_binary(request->buff, &index, fn, &length); fn[length] = 0;
    Str s; string_new_len(&s, fn, length); from_string(&args[0], &s);
    if (ei_decode_list_header(request->buff, &index, &count) || count > MAX_ARGS ||
        decode_args(request->buff, &index, count, &args[1], err, sizeof err)) {
      reply(fd, message, ref, ref_size, "error", err[0] ? err : "bad argument list", NULL);
    } else {
      object_variant(sandbox, &self);
      if (call_method(&self, "vmcall", args, count + 1, &ret, err, sizeof err)) reply(fd, message, ref, ref_size, "error", err, NULL);
      else { char line[300]; snprintf(line, sizeof line, "vmcall %s(%d args) ok", fn, count); console_log(line, "info"); ei_x_buff term; ei_x_new(&term); encode_variant(&term, &ret); reply(fd, message, ref, ref_size, "ok", NULL, &term); ei_x_free(&term); variant_destroy(&ret); }
    }
    free(fn);
  } else reply(fd, message, ref, ref_size, "error", "unknown command", NULL);
  free(ref);
}

static void iterate_engine(void) {
  Var self, ret; char err[128];
  object_variant(instance, &self);
  if (call_method(&self, "iteration", NULL, 0, &ret, err, sizeof err) == 0) variant_destroy(&ret);
}

static void serve_connection(int fd) {
  erlang_msg message;
  ei_x_buff request;
  ei_x_new(&request);
  for (;;) {
    request.index = 0;
    int result = ei_xreceive_msg_tmo(fd, &message, &request, 5);
    if (result == ERL_TIMEOUT) { iterate_engine(); continue; }
    if (result == ERL_ERROR) break;
    if (result == ERL_MSG && (message.msgtype == ERL_SEND || message.msgtype == ERL_REG_SEND)) handle(fd, &message, &request);
  }
  ei_x_free(&request);
}

int main(int argc, char **argv) {
  const char *cookie = getenv("GUEST_CNODE_COOKIE");
  const char *name = getenv("GUEST_CNODE_NAME");
  const char *library = getenv("LIBGODOT_PATH");
  if (!cookie || argc < 2) { fprintf(stderr, "usage: GUEST_CNODE_COOKIE=c [GUEST_CNODE_NAME=guest] guest-cnode <godot args...>\n"); return 2; }
  if (!name) name = "guest";
  void *handle_lib = dlopen(library ? library : "libgodot.so", RTLD_NOW | RTLD_GLOBAL);
  if (!handle_lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
  create_fn create = dlsym(handle_lib, "libgodot_create_godot_instance");
  if (!create) { fprintf(stderr, "libgodot_create_godot_instance missing\n"); return 1; }
  char **godot_argv = calloc((size_t)argc + 1, sizeof *godot_argv);
  godot_argv[0] = argv[0];
  for (int i = 1; i < argc; i++) godot_argv[i] = argv[i];
  instance = create(argc, godot_argv, init_ext);
  if (!instance) { fprintf(stderr, "libgodot instance not created\n"); return 1; }
  bind_interface();
  { Var self, ret; char err[128]; object_variant(instance, &self);
    if (call_method(&self, "start", NULL, 0, &ret, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; } variant_destroy(&ret); }
  struct in_addr loopback; inet_pton(AF_INET, HOST, &loopback);
  char full[128]; snprintf(full, sizeof full, "%s@" HOST, name);
  ei_cnode node; ei_init();
  if (ei_connect_xinit(&node, HOST, name, full, &loopback, cookie, 0) < 0) return 1;
  struct sockaddr_in address = {.sin_family = AF_INET}; socklen_t length = sizeof address;
  inet_pton(AF_INET, HOST, &address.sin_addr);
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if (bind(listener, (struct sockaddr *)&address, sizeof address) || listen(listener, 5)) return 1;
  getsockname(listener, (struct sockaddr *)&address, &length);
  if (ei_publish(&node, ntohs(address.sin_port)) == -1) { fprintf(stderr, "epmd not running\n"); return 1; }
  fprintf(stderr, "%s ready\n", full);
  for (;;) {
    ErlConnect conn;
    int fd = ei_accept(&node, listener, &conn);
    if (fd < 0) continue;
    serve_connection(fd);
    close(fd);
  }
}
