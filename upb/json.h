/*
** string <-> string transcoding of JSON <-> protobuf binary.
**
*/

#ifndef UPB_JSON_H_
#define UPB_JSON_H_

#include "upb/def.h"

struct upb_jsonparser;
typedef struct upb_jsonparser upb_jsonparser;

/* Initializes a JSON parser state in |outbuf| (the end of the buffer is used
 * for bookkeeping).  Serialized bytes will be written to outbuf.  The top-level
 * message is indicated by |m| and "Any" messages will be looked up in
 * |any_msgs|.  If an error occurs, it will be written to |status|.  */
upb_jsonparser* upb_jsonparser_init(char* outbuf, size_t len,
                                    const upb_msgdef* m,
                                    const upb_symtab* any_msgs,
                                    upb_status* status);

/* Indicates that the next buffer passed to upb_jsontobinary() is not the final
 * buffer, so we should not assume that end of buffer is end of file.  This flag
 * is cleared every time you call upb_jsontobinary(), so you must reset it for
 * every input buffer that is not the final buffer. */
void upb_jsonparser_setpartial(upb_jsonparser* out);

/* Parses JSON data from the given buffer and writes serialized protobuf binary
 * data to |outbuf| as provided when |out| was initialized.  The number of bytes
 * written is stored in |*outbytes}.  Returns the number of bytes parsed.  Check
 * |status| to see if an error occurred. */
size_t upb_jsontobinary(const char* buf, size_t len, upb_jsonparser* out,
                        size_t* outbytes);

char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen);

#endif  /* UPB_JSON_H */
