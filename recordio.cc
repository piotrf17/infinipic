// Modified from: https://code.google.com/p/or-tools/

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

#include "recordio.h"

#include <google/protobuf/message.h>

namespace file {

const int RecordWriter::kMagicNumber = 0x3ed7230a;

RecordWriter::RecordWriter(std::ofstream* const file)
    : file_(file) {
}

bool RecordWriter::WriteProtocolMessage(
    const google::protobuf::MessageLite& message) {
  return WriteRecord(message.SerializeAsString());
}

bool RecordWriter::WriteRecord(const std::string& data) {
  return WriteRecord(data.data(), data.size());
}

bool RecordWriter::WriteRecord(const char* buffer, size_t len) {
  file_->write(reinterpret_cast<const char*>(&kMagicNumber),
               sizeof(kMagicNumber));
  file_->write(reinterpret_cast<const char*>(&len), sizeof(len));
  file_->write(buffer, len);
  return !file_->fail();
}


bool RecordWriter::Close() {
  file_->close();
  return file_->fail();
}

RecordReader::RecordReader(std::ifstream* const file)
    : file_(file) {
}

bool RecordReader::ReadProtocolMessage(
    google::protobuf::MessageLite* message) {
  size_t size = 0;
  const char* buffer;
  if (!ReadRecord(&buffer, &size)) {
    return false;
  }
  if (!message->ParseFromArray(buffer, size)) {
    return false;
  }
  delete[] buffer;
  return true;
}

bool RecordReader::ReadRecord(std::string* data) {
  size_t size = 0;
  const char* buffer;
  if (!ReadRecord(&buffer, &size)) {
    return false;
  }
  data->assign(buffer, size);
  delete[] buffer;
  return true;
}

bool RecordReader::ReadRecord(const char** buffer, size_t* len) {
  int magic_number = 0;
  *buffer = nullptr;
  file_->read(reinterpret_cast<char*>(&magic_number),
              sizeof(magic_number));
  if (file_->fail() || (magic_number != RecordWriter::kMagicNumber)) {
    return false;
  }
  file_->read(reinterpret_cast<char*>(len), sizeof(*len));
  if (file_->fail()) {
    return false;
  }
  char* data = new char[*len];
  file_->read(data, *len);
  if (file_->fail()) {
    delete[] data;
    return false;
  }
  *buffer = data;
  return true;
}

bool RecordReader::Close() {
  file_->close();
  return file_->fail();
}

bool RecordReader::ReadRecordSized(char* buffer, size_t len) {
  int magic_number = 0;
  file_->read(reinterpret_cast<char*>(&magic_number),
              sizeof(magic_number));
  if (file_->fail() || (magic_number != RecordWriter::kMagicNumber)) {
    return false;
  }
  size_t read_len;
  file_->read(reinterpret_cast<char*>(&read_len), sizeof(read_len));
  if (file_->fail() || (read_len != len)) {
    return false;
  }
  file_->read(buffer, len);
  return !file_->fail();
}

}  // namespace file
