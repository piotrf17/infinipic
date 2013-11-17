// Record input and output - classes to append / read proto buffers from
// binary files.  Modified from: https://code.google.com/p/or-tools/
//
// These RecordIO implementations only have a minimal safety against corruption
// in the form of a magic number written with every record.  Specifically, no
// checksums are computed.

// Copyright 2011 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INFINIPIC_RECORDIO_H_
#define INFINIPIC_RECORDIO_H_

#include <fstream>
#include <memory>
#include <string>
#include <type_traits>

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace file {

// This class appends a protocol buffer to a file in a binary format.
class RecordWriter {
 public:
  static const int kMagicNumber;

  // Write to the provided file.  RecordWriter does not take ownership of
  // the file.
  explicit RecordWriter(std::ofstream* const file);

  // Convenience method for directly writing a protocol buffer.
  bool WriteProtocolMessage(const google::protobuf::MessageLite& message);
  
  // Write a single record of the data contained in the given string.
  bool WriteRecord(const std::string& data);

  // Write a single record in buffer of size len.
  bool WriteRecord(const char* buffer, size_t len);

  // Write a single record of a given type.
  template <typename T>
  bool Write(const T& t);
  
  // Close the underlying file.  Any further calls to Write* are undefined.
  bool Close();

 private:
  std::ofstream* const file_;
};

// This class reads a protocol buffer from a file.
class RecordReader {
 public:
  // Read from the provided file.  RecordReader does not take ownership
  // of the file.
  explicit RecordReader(std::ifstream* const file);

  // Convenience method for directly reading a protocol buffer.
  bool ReadProtocolMessage(google::protobuf::MessageLite* message);

  // Read a single record into the given string.
  bool ReadRecord(std::string* data);

  // Read a single record, storing the result in buffer.  The size of read data
  // is returned in len.  Caller assumes ownership of the data in buffer.
  bool ReadRecord(const char** buffer, size_t* len);

  // Read a single record of a given type, only works for POD.
  template <typename T>
  bool Read(T* t);

  // Close the underlying file.  Any further calls to Read* are undefined.
  bool Close();

 private:
  bool ReadRecordSized(char* buffer, size_t len);
  
  std::ifstream* const file_;
};

template <typename T>
bool RecordWriter::Write(const T& t) {
  return WriteRecord(reinterpret_cast<const char*>(&t), sizeof(T));
}

template <typename T>
bool RecordReader::Read(T* t) {
  static_assert(std::is_pod<T>::value,
                "Can only use RecordReader::Read on POD");
  return ReadRecordSized(reinterpret_cast<char*>(t), sizeof(T));
}

}  // namespace file

#endif  // INFINIPIC_RECORDIO_H_
