/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (task_http_harness_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>

#include "../../../msg_hash.h"
#include "../../../tasks/tasks_internal.h"
#include "../task_harness.h"

struct http_connection_t
{
   char *url;
   char *method;
   char *headers;
   char *user_agent;
   char *content_type;
   void *content;
   size_t content_length;
   unsigned iterate_calls;
};

struct http_t
{
   char *body;
   size_t body_len;
   int status;
   bool error;
   unsigned update_calls;
   unsigned done_after_updates;
};

typedef struct
{
   const char *body;
   const char *header;
   int status;
   bool error;
   unsigned done_after_updates;
} fake_http_script_t;

typedef struct
{
   unsigned callbacks;
   int status;
   int progress;
   bool saw_error;
   char error[128];
   char body[128];
   size_t body_len;
   size_t header_count;
} callback_capture_t;

static int failures;
static fake_http_script_t fake_script;
static unsigned fake_connection_new_calls;
static unsigned fake_connection_free_calls;
static unsigned fake_http_new_calls;
static unsigned fake_http_delete_calls;
static unsigned fake_update_calls;

retro_time_t cpu_features_get_time_usec(void)
{
   return 1;
}

bool network_init(void)
{
   return true;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "Downloading";
}

void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

static char *fake_strdup(const char *value)
{
   size_t len;
   char *copy;

   if (!value)
      return NULL;

   len  = strlen(value) + 1;
   copy = (char*)malloc(len);
   if (copy)
      memcpy(copy, value, len);

   return copy;
}

static void fake_reset(const fake_http_script_t *script)
{
   memset(&fake_script, 0, sizeof(fake_script));
   if (script)
      fake_script = *script;
   fake_connection_new_calls  = 0;
   fake_connection_free_calls = 0;
   fake_http_new_calls        = 0;
   fake_http_delete_calls     = 0;
   fake_update_calls          = 0;
}

static struct http_connection_t *fake_connection_new(
      const char *url, const char *method, const char *data)
{
   struct http_connection_t *conn =
      (struct http_connection_t*)calloc(1, sizeof(*conn));

   (void)data;

   if (!conn)
      return NULL;

   conn->url    = fake_strdup(url);
   conn->method = fake_strdup(method);
   fake_connection_new_calls++;

   return conn;
}

static bool fake_connection_iterate(struct http_connection_t *conn)
{
   conn->iterate_calls++;
   return true;
}

static bool fake_connection_done(struct http_connection_t *conn)
{
   return conn && conn->iterate_calls > 0;
}

static void fake_connection_free(struct http_connection_t *conn)
{
   if (!conn)
      return;

   free(conn->url);
   free(conn->method);
   free(conn->headers);
   free(conn->user_agent);
   free(conn->content_type);
   free(conn->content);
   free(conn);
   fake_connection_free_calls++;
}

static void fake_connection_set_user_agent(
      struct http_connection_t *conn, const char *user_agent)
{
   if (conn)
      conn->user_agent = fake_strdup(user_agent);
}

static void fake_connection_set_headers(
      struct http_connection_t *conn, const char *headers)
{
   if (conn)
      conn->headers = fake_strdup(headers);
}

static void fake_connection_set_content(struct http_connection_t *conn,
      const char *content_type, size_t content_length, const void *content)
{
   if (!conn)
      return;

   conn->content_type    = fake_strdup(content_type);
   conn->content_length  = content_length;
   if (content && content_length)
   {
      conn->content = malloc(content_length);
      if (conn->content)
         memcpy(conn->content, content, content_length);
   }
}

static const char *fake_connection_url(struct http_connection_t *conn)
{
   return conn ? conn->url : NULL;
}

static const char *fake_connection_method(struct http_connection_t *conn)
{
   return conn ? conn->method : NULL;
}

static struct http_t *fake_http_new(struct http_connection_t *conn)
{
   struct http_t *http = (struct http_t*)calloc(1, sizeof(*http));

   (void)conn;

   if (!http)
      return NULL;

   http->body               = fake_strdup(fake_script.body);
   http->body_len           = http->body ? strlen(http->body) : 0;
   http->status             = fake_script.status;
   http->error              = fake_script.error;
   http->done_after_updates = fake_script.done_after_updates
      ? fake_script.done_after_updates : 1;
   fake_http_new_calls++;

   return http;
}

static bool fake_http_update(struct http_t *http,
      size_t *progress, size_t *total)
{
   http->update_calls++;
   fake_update_calls++;

   *total    = 10;
   *progress = http->update_calls >= http->done_after_updates
      ? 10 : http->update_calls * 5;

   return http->update_calls >= http->done_after_updates;
}

static int fake_http_status(struct http_t *http)
{
   return http ? http->status : 0;
}

static bool fake_http_error(struct http_t *http)
{
   return http && http->error;
}

static struct string_list *fake_http_headers_ex(
      struct http_t *http, bool accept_error)
{
   union string_list_elem_attr attr;
   struct string_list *headers;

   (void)http;
   (void)accept_error;

   if (!fake_script.header)
      return NULL;

   headers = string_list_new();
   if (!headers)
      return NULL;

   attr.i = 0;
   if (!string_list_append(headers, fake_script.header, attr))
   {
      string_list_free(headers);
      return NULL;
   }

   return headers;
}

static uint8_t *fake_http_data(struct http_t *http,
      size_t *len, bool accept_error)
{
   uint8_t *data;

   (void)accept_error;

   if (!http || !http->body)
      return NULL;

   data = (uint8_t*)fake_strdup(http->body);
   if (len)
      *len = http->body_len;

   return data;
}

static void fake_http_delete(struct http_t *http)
{
   if (!http)
      return;

   free(http->body);
   free(http);
   fake_http_delete_calls++;
}

static const struct task_http_net_driver fake_driver = {
   fake_connection_new,
   fake_connection_iterate,
   fake_connection_done,
   fake_connection_free,
   fake_connection_set_user_agent,
   fake_connection_set_headers,
   fake_connection_set_content,
   fake_connection_url,
   fake_connection_method,
   fake_http_new,
   fake_http_update,
   fake_http_status,
   fake_http_error,
   fake_http_headers_ex,
   fake_http_data,
   fake_http_delete
};

static void capture_http_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   callback_capture_t *capture = (callback_capture_t*)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;

   capture->callbacks++;
   capture->progress  = task_get_progress(task);
   capture->saw_error = error && *error;

   if (error)
      strlcpy(capture->error, error, sizeof(capture->error));

   if (data)
   {
      capture->status   = data->status;
      capture->body_len = data->len;
      capture->header_count = data->headers ? data->headers->size : 0;
      if (data->data)
         strlcpy(capture->body, data->data, sizeof(capture->body));
   }
}

static bool capture_done(void *data)
{
   callback_capture_t *capture = (callback_capture_t*)data;
   return capture->callbacks > 0;
}

static void expect_true(const char *name, bool value)
{
   if (!value)
   {
      printf("[FAILED] %s\n", name);
      failures++;
   }
   else
      printf("[SUCCESS] %s\n", name);
}

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      printf("[FAILED] %s: expected %d, got %d\n", name, want, got);
      failures++;
   }
   else
      printf("[SUCCESS] %s\n", name);
}

static void expect_string(const char *name, const char *got, const char *want)
{
   if (strcmp(got ? got : "", want ? want : "") != 0)
   {
      printf("[FAILED] %s: expected '%s', got '%s'\n",
            name, want ? want : "", got ? got : "");
      failures++;
   }
   else
      printf("[SUCCESS] %s\n", name);
}

static void test_successful_get_runs_through_queue(void)
{
   callback_capture_t capture;
   fake_http_script_t script = {"payload", "X-Test: ok", 200, false, 2};

   memset(&capture, 0, sizeof(capture));
   fake_reset(&script);
   task_harness_init();
   task_http_set_net_driver(&fake_driver);

   expect_true("GET task is queued",
         task_push_http_transfer("https://example.test/file", false, NULL,
            capture_http_callback, &capture) != NULL);
   expect_true("GET task finishes",
         task_harness_run_until(capture_done, &capture, 8));

   expect_int("callback count", (int)capture.callbacks, 1);
   expect_int("HTTP status copied", capture.status, 200);
   expect_string("body copied", capture.body, "payload");
   expect_int("body length copied", (int)capture.body_len, 7);
   expect_int("response headers copied", (int)capture.header_count, 1);
   expect_int("transfer update was ticked twice", (int)fake_update_calls, 2);
   expect_int("progress captured before completion", capture.progress, 50);
   expect_int("connection freed", (int)fake_connection_free_calls, 1);
   expect_int("HTTP handle deleted", (int)fake_http_delete_calls, 1);

   task_http_reset_net_driver();
   task_harness_deinit();
}

static void test_duplicate_get_is_rejected_while_running(void)
{
   callback_capture_t capture;
   fake_http_script_t script = {"payload", NULL, 200, false, 3};

   memset(&capture, 0, sizeof(capture));
   fake_reset(&script);
   task_harness_init();
   task_http_set_net_driver(&fake_driver);

   expect_true("first GET is queued",
         task_push_http_transfer("https://example.test/dup", false, NULL,
            capture_http_callback, &capture) != NULL);
   expect_true("duplicate GET is rejected",
         task_push_http_transfer("https://example.test/dup", false, NULL,
            capture_http_callback, &capture) == NULL);
   expect_true("remaining GET finishes",
         task_harness_run_until(capture_done, &capture, 10));
   expect_int("duplicate connection was freed", (int)fake_connection_free_calls, 2);
   expect_int("only one HTTP transfer started", (int)fake_http_new_calls, 1);

   task_http_reset_net_driver();
   task_harness_deinit();
}

static void test_duplicate_post_is_allowed(void)
{
   callback_capture_t capture;
   fake_http_script_t script = {"created", NULL, 201, false, 1};

   memset(&capture, 0, sizeof(capture));
   fake_reset(&script);
   task_harness_init();
   task_http_set_net_driver(&fake_driver);

   expect_true("first POST is queued",
         task_push_http_post_transfer("https://example.test/post", "a=1",
            false, NULL, capture_http_callback, &capture) != NULL);
   expect_true("second POST is queued",
         task_push_http_post_transfer("https://example.test/post", "a=2",
            false, NULL, capture_http_callback, &capture) != NULL);

   task_harness_run_ticks(8);
   expect_int("both POST callbacks ran", (int)capture.callbacks, 2);
   expect_int("both POST transfers started", (int)fake_http_new_calls, 2);

   task_http_reset_net_driver();
   task_harness_deinit();
}

static void test_transport_error_sets_task_error(void)
{
   callback_capture_t capture;
   fake_http_script_t script = {NULL, NULL, 503, true, 1};

   memset(&capture, 0, sizeof(capture));
   fake_reset(&script);
   task_harness_init();
   task_http_set_net_driver(&fake_driver);

   expect_true("erroring GET is queued",
         task_push_http_transfer("https://example.test/fail", false, NULL,
            capture_http_callback, &capture) != NULL);
   expect_true("erroring GET finishes",
         task_harness_run_until(capture_done, &capture, 8));
   expect_true("task error is reported", capture.saw_error);
   expect_string("error text", capture.error, "Download failed.");
   expect_int("HTTP status copied on error", capture.status, 503);

   task_http_reset_net_driver();
   task_harness_deinit();
}

static void test_cancel_after_connect_reports_cancelled(void)
{
   callback_capture_t capture;
   fake_http_script_t script = {"unused", NULL, 200, false, 4};
   void *task;

   memset(&capture, 0, sizeof(capture));
   fake_reset(&script);
   task_harness_init();
   task_http_set_net_driver(&fake_driver);

   task = task_push_http_transfer("https://example.test/cancel", false,
         NULL, capture_http_callback, &capture);
   expect_true("cancellable GET is queued", task != NULL);
   task_harness_run_ticks(2);
   task_queue_cancel_task(task);
   expect_true("cancelled GET finishes",
         task_harness_run_until(capture_done, &capture, 8));
   expect_true("cancelled task reports an error", capture.saw_error);
   expect_string("cancel error text", capture.error, "Task cancelled.");
   expect_int("cancelled HTTP handle deleted", (int)fake_http_delete_calls, 1);

   task_http_reset_net_driver();
   task_harness_deinit();
}

int main(void)
{
   test_successful_get_runs_through_queue();
   test_duplicate_get_is_rejected_while_running();
   test_duplicate_post_is_allowed();
   test_transport_error_sets_task_error();
   test_cancel_after_connect_reports_cancelled();

   if (failures)
   {
      printf("\n%d task HTTP harness test(s) failed\n", failures);
      return 1;
   }

   printf("\nAll task HTTP harness tests passed.\n");
   return 0;
}
