// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_FORM_DATA_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_FORM_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/data_reader.h>

namespace chromeos {
namespace http {

namespace content_disposition {
CHROMEOS_EXPORT extern const char kFormData[];
CHROMEOS_EXPORT extern const char kFile[];
}  // namespace content_disposition

// An abstract base class for all types of form fields used by FormData class.
// This class represents basic information about a form part in
// multipart/form-data and multipart/mixed content.
// For more details on multipart content, see the following RFC:
//    http://www.ietf.org/rfc/rfc2388
// For more details on MIME and content headers, see the following RFC:
//    http://www.ietf.org/rfc/rfc2045
class CHROMEOS_EXPORT FormField : public DataReaderInterface {
 public:
  // The constructor that takes the basic data part information common to
  // all part types. An example of part's headers could include:
  //
  //    Content-Disposition: form-data; name="field1"
  //    Content-Type: text/plain;charset=windows-1250
  //    Content-Transfer-Encoding: quoted-printable
  //
  // The constructor parameters correspond to the basic part attributes:
  //  |name| = the part name ("name" parameter of Content-Disposition header;
  //           "field1" in the example above)
  //  |content_disposition| = the part disposition ("form-data" in the example)
  //  |content_type| = the content type ("text/plain;charset=windows-1250")
  //  |transfer_encoding| = the encoding type for transport ("quoted-printable")
  //                        See http://www.ietf.org/rfc/rfc2045, section 6.1
  FormField(const std::string& name,
            const std::string& content_disposition,
            const std::string& content_type,
            const std::string& transfer_encoding);

  // Returns the full Content-Disposition header value. This might include the
  // disposition type itself as well as the field "name" and/or "filename"
  // parameters.
  virtual std::string GetContentDisposition() const;

  // Returns the full content type of field data. MultiPartFormField overloads
  // this method to append "boundary" parameter to it.
  virtual std::string GetContentType() const;

  // Returns a string with all of the field headers, delimited by CRLF
  // characters ("\r\n").
  std::string GetContentHeader() const;

 protected:
  // Form field name. If not empty, it will be appended to Content-Disposition
  // field header using "name" attribute.
  std::string name_;

  // Form field disposition. Most of the time this will be "form-data". But for
  // nested file uploads inside "multipart/mixed" sections, this can be "file".
  std::string content_disposition_;

  // Content type. If omitted (empty), "plain/text" assumed.
  std::string content_type_;

  // Transfer encoding for field data. If omitted, "7bit" is assumed. For most
  // binary contents (e.g. for file content), use "binary".
  std::string transfer_encoding_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormField);
};

// Simple text form field.
class CHROMEOS_EXPORT TextFormField : public FormField {
 public:
  // Constructor. Parameters:
  //  name: field name
  //  data: field text data
  //  content_type: the data content type. Empty if not specified.
  //  transfer_encoding: the encoding type of data. If omitted, no encoding
  //      is specified (and "7bit" is assumed).
  TextFormField(const std::string& name,
                const std::string& data,
                const std::string& content_type = {},
                const std::string& transfer_encoding = {});

  // Overrides from DataReaderInterface.
  uint64_t GetDataSize() const override;
  bool ReadData(void* buffer,
                size_t size_to_read,
                size_t* size_read,
                chromeos::ErrorPtr* error) override;

 private:
  MemoryDataReader data_;  // Buffer/reader for field data.

  DISALLOW_COPY_AND_ASSIGN(TextFormField);
};

// File upload form field.
class CHROMEOS_EXPORT FileFormField : public FormField {
 public:
  // Constructor. Parameters:
  //  name: field name
  //  file: open file descriptor with the contents of the file.
  //  file_name: just the base file name of the file, e.g. "file.txt".
  //      Used in "filename" parameter of Content-Disposition header.
  //  content_type: valid content type of the file.
  //  transfer_encoding: the encoding type of data.
  //      If omitted, "binary" is used.
  FileFormField(const std::string& name,
                base::File file,
                const std::string& file_name,
                const std::string& content_disposition,
                const std::string& content_type,
                const std::string& transfer_encoding = {});

  // Override from FormField.
  // Appends "filename" parameter to Content-Disposition header.
  std::string GetContentDisposition() const override;

  // Overrides from DataReaderInterface.
  uint64_t GetDataSize() const override;
  bool ReadData(void* buffer,
                size_t size_to_read,
                size_t* size_read,
                chromeos::ErrorPtr* error) override;

 private:
  base::File file_;
  std::string file_name_;

  DISALLOW_COPY_AND_ASSIGN(FileFormField);
};

// Multipart form field.
// This is used directly by FormData class to build the request body for
// form upload. It can also be used with multiple file uploads for a single
// file field, when the uploaded files should be sent as "multipart/mixed".
class CHROMEOS_EXPORT MultiPartFormField : public FormField {
 public:
  // Constructor. Parameters:
  //  name: field name
  //  content_type: valid content type. If omitted, "multipart/mixed" is used.
  //  boundary: multipart boundary separator.
  //      If omitted/empty, a random string is generated.
  MultiPartFormField(const std::string& name,
                     const std::string& content_type = {},
                     const std::string& boundary = {});

  // Override from FormField.
  // Appends "boundary" parameter to Content-Type header.
  std::string GetContentType() const override;

  // Overrides from DataReaderInterface.
  uint64_t GetDataSize() const override;
  bool ReadData(void* buffer,
                size_t size_to_read,
                size_t* size_read,
                chromeos::ErrorPtr* error) override;

  // Adds a form field to the form data. The |field| could be a simple text
  // field, a file upload field or a multipart form field.
  void AddCustomField(std::unique_ptr<FormField> field);

  // Adds a simple text form field.
  void AddTextField(const std::string& name,
                    const std::string& data);

  // Adds a file upload form field using a file path.
  bool AddFileField(const std::string& name,
                    const base::FilePath& file_path,
                    const std::string& content_disposition,
                    const std::string& content_type,
                    chromeos::ErrorPtr* error);

  // Returns a boundary string used to separate multipart form fields.
  const std::string& GetBoundary() const { return boundary_; }

 private:
  enum class ReadStage {
    kStart,          // We haven't read any data from this FormField yet.
    kBoundarySetup,  // Fills a local buffer with data for the current boundary
                     // and headers for the next section (or simply the final
                     // boundary if we have no more parts).
    kBoundaryData,   // Read data from our local buffer before reading the
                     // actual data of the current form section.
    kPart,           // Read data from the form section we're in.
    kEnd,            // No more data to read.
  };

  // Returns the starting boundary string: "--<boundary>".
  std::string GetBoundaryStart() const;
  // Returns the ending boundary string: "--<boundary>--".
  std::string GetBoundaryEnd() const;

  std::string boundary_;  // Boundary string used as field separator.
  std::vector<std::unique_ptr<FormField>> parts_;  // Form field list.

  // Internal implementation data for ReadData() method.
  ReadStage read_stage_{ReadStage::kStart};
  size_t read_part_index_{0};
  MemoryDataReader boundary_reader_;

  DISALLOW_COPY_AND_ASSIGN(MultiPartFormField);
};

// A class representing a multipart form data for sending as HTTP POST request.
class CHROMEOS_EXPORT FormData final : public DataReaderInterface {
 public:
  FormData();
  // Allows to specify a custom |boundary| separator string.
  explicit FormData(const std::string& boundary);

  // Adds a form field to the form data. The |field| could be a simple text
  // field, a file upload field or a multipart form field.
  void AddCustomField(std::unique_ptr<FormField> field);

  // Adds a simple text form field.
  void AddTextField(const std::string& name,
                    const std::string& data);

  // Adds a file upload form field using a file path.
  bool AddFileField(const std::string& name,
                    const base::FilePath& file_path,
                    const std::string& content_type,
                    chromeos::ErrorPtr* error);

  // Returns the complete content type string to be used in HTTP requests.
  std::string GetContentType() const;

  // Overrides from DataReaderInterface.
  uint64_t GetDataSize() const override;
  bool ReadData(void* buffer,
                size_t size_to_read,
                size_t* size_read,
                chromeos::ErrorPtr* error) override;

 private:
  MultiPartFormField form_data_;

  DISALLOW_COPY_AND_ASSIGN(FormData);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_FORM_DATA_H_
