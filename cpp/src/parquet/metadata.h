// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef PARQUET_FILE_METADATA_H
#define PARQUET_FILE_METADATA_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "arrow/util/key_value_metadata.h"
#include "arrow/util/macros.h"

#include "parquet/properties.h"
#include "parquet/types.h"
#include "parquet/util/visibility.h"

namespace parquet {

class ColumnDescriptor;
class EncodedStatistics;
class OutputStream;
class Statistics;
class SchemaDescriptor;

namespace schema {

class ColumnPath;

}  // namespace schema

using KeyValueMetadata = ::arrow::KeyValueMetadata;

class PARQUET_EXPORT ApplicationVersion {
 public:
  // Known Versions with Issues
  static const ApplicationVersion& PARQUET_251_FIXED_VERSION();
  static const ApplicationVersion& PARQUET_816_FIXED_VERSION();
  static const ApplicationVersion& PARQUET_CPP_FIXED_STATS_VERSION();
  static const ApplicationVersion& PARQUET_MR_FIXED_STATS_VERSION();
  // Regular expression for the version format
  // major . minor . patch unknown - prerelease.x + build info
  // Eg: 1.5.0ab-cdh5.5.0+cd
  static constexpr char const* VERSION_FORMAT =
      "^(\\d+)\\.(\\d+)\\.(\\d+)([^-+]*)?(?:-([^+]*))?(?:\\+(.*))?$";
  // Regular expression for the application format
  // application_name version VERSION_FORMAT (build build_name)
  // Eg: parquet-cpp version 1.5.0ab-xyz5.5.0+cd (build abcd)
  static constexpr char const* APPLICATION_FORMAT =
      "(.*?)\\s*(?:(version\\s*(?:([^(]*?)\\s*(?:\\(\\s*build\\s*([^)]*?)\\s*\\))?)?)?)";

  // Application that wrote the file. e.g. "IMPALA"
  std::string application_;
  // Build name
  std::string build_;

  // Version of the application that wrote the file, expressed as
  // (<major>.<minor>.<patch>). Unmatched parts default to 0.
  // "1.2.3"    => {1, 2, 3}
  // "1.2"      => {0, 0, 0}
  // "1.2-cdh5" => {0, 0, 0}
  // TODO (majetideepak): Implement support for pre_release
  struct {
    int major;
    int minor;
    int patch;
    std::string unknown;
    std::string pre_release;
    std::string build_info;
  } version;

  ApplicationVersion() {}
  explicit ApplicationVersion(const std::string& created_by);
  ApplicationVersion(const std::string& application, int major, int minor, int patch);

  // Returns true if version is strictly less than other_version
  bool VersionLt(const ApplicationVersion& other_version) const;

  // Returns true if version is strictly less than other_version
  bool VersionEq(const ApplicationVersion& other_version) const;

  // Checks if the Version has the correct statistics for a given column
  bool HasCorrectStatistics(Type::type primitive, EncodedStatistics& statistics,
                            SortOrder::type sort_order = SortOrder::SIGNED) const;
};

class PARQUET_EXPORT ColumnChunkMetaData {
 public:
  // API convenience to get a MetaData accessor
  static std::unique_ptr<ColumnChunkMetaData> Make(
      const void* metadata, const ColumnDescriptor* descr,
      const ApplicationVersion* writer_version = NULLPTR);

  ~ColumnChunkMetaData();

  // column chunk
  int64_t file_offset() const;

  // parameter is only used when a dataset is spread across multiple files
  const std::string& file_path() const;

  // column metadata
  Type::type type() const;
  int64_t num_values() const;
  std::shared_ptr<schema::ColumnPath> path_in_schema() const;
  bool is_stats_set() const;
  std::shared_ptr<Statistics> statistics() const;
  Compression::type compression() const;
  const std::vector<Encoding::type>& encodings() const;
  bool has_dictionary_page() const;
  int64_t dictionary_page_offset() const;
  int64_t data_page_offset() const;
  bool has_index_page() const;
  int64_t index_page_offset() const;
  int64_t total_compressed_size() const;
  int64_t total_uncompressed_size() const;

 private:
  explicit ColumnChunkMetaData(const void* metadata, const ColumnDescriptor* descr,
                               const ApplicationVersion* writer_version = NULLPTR);
  // PIMPL Idiom
  class ColumnChunkMetaDataImpl;
  std::unique_ptr<ColumnChunkMetaDataImpl> impl_;
};

class PARQUET_EXPORT RowGroupMetaData {
 public:
  // API convenience to get a MetaData accessor
  static std::unique_ptr<RowGroupMetaData> Make(
      const void* metadata, const SchemaDescriptor* schema,
      const ApplicationVersion* writer_version = NULLPTR);

  ~RowGroupMetaData();

  // row-group metadata
  int num_columns() const;
  int64_t num_rows() const;
  int64_t total_byte_size() const;
  // Return const-pointer to make it clear that this object is not to be copied
  const SchemaDescriptor* schema() const;
  std::unique_ptr<ColumnChunkMetaData> ColumnChunk(int i) const;

 private:
  explicit RowGroupMetaData(const void* metadata, const SchemaDescriptor* schema,
                            const ApplicationVersion* writer_version = NULLPTR);
  // PIMPL Idiom
  class RowGroupMetaDataImpl;
  std::unique_ptr<RowGroupMetaDataImpl> impl_;
};

class FileMetaDataBuilder;

class PARQUET_EXPORT FileMetaData {
 public:
  // API convenience to get a MetaData accessor
  static std::shared_ptr<FileMetaData> Make(const void* serialized_metadata,
                                            uint32_t* metadata_len);

  ~FileMetaData();

  // file metadata
  uint32_t size() const;
  int num_columns() const;
  int64_t num_rows() const;
  int num_row_groups() const;
  ParquetVersion::type version() const;
  const std::string& created_by() const;
  int num_schema_elements() const;
  std::unique_ptr<RowGroupMetaData> RowGroup(int i) const;

  const ApplicationVersion& writer_version() const;

  void WriteTo(OutputStream* dst) const;

  // Return const-pointer to make it clear that this object is not to be copied
  const SchemaDescriptor* schema() const;

  std::shared_ptr<const KeyValueMetadata> key_value_metadata() const;

  // Set file_path ColumnChunk fields to a particular value
  void set_file_path(const std::string& path);

 private:
  friend FileMetaDataBuilder;
  explicit FileMetaData(const void* serialized_metadata, uint32_t* metadata_len);

  // PIMPL Idiom
  FileMetaData();
  class FileMetaDataImpl;
  std::unique_ptr<FileMetaDataImpl> impl_;
};

// Builder API
class PARQUET_EXPORT ColumnChunkMetaDataBuilder {
 public:
  // API convenience to get a MetaData reader
  static std::unique_ptr<ColumnChunkMetaDataBuilder> Make(
      const std::shared_ptr<WriterProperties>& props, const ColumnDescriptor* column);

  static std::unique_ptr<ColumnChunkMetaDataBuilder> Make(
      const std::shared_ptr<WriterProperties>& props, const ColumnDescriptor* column,
      void* contents);

  ~ColumnChunkMetaDataBuilder();

  // column chunk
  // Used when a dataset is spread across multiple files
  void set_file_path(const std::string& path);
  // column metadata
  void SetStatistics(const EncodedStatistics& stats);
  // get the column descriptor
  const ColumnDescriptor* descr() const;
  // commit the metadata
  void Finish(int64_t num_values, int64_t dictonary_page_offset,
              int64_t index_page_offset, int64_t data_page_offset,
              int64_t compressed_size, int64_t uncompressed_size, bool has_dictionary,
              bool dictionary_fallback);

  // The metadata contents, suitable for passing to ColumnChunkMetaData::Make
  const void* contents() const;

  // For writing metadata at end of column chunk
  void WriteTo(OutputStream* sink);

 private:
  explicit ColumnChunkMetaDataBuilder(const std::shared_ptr<WriterProperties>& props,
                                      const ColumnDescriptor* column);
  explicit ColumnChunkMetaDataBuilder(const std::shared_ptr<WriterProperties>& props,
                                      const ColumnDescriptor* column, void* contents);
  // PIMPL Idiom
  class ColumnChunkMetaDataBuilderImpl;
  std::unique_ptr<ColumnChunkMetaDataBuilderImpl> impl_;
};

class PARQUET_EXPORT RowGroupMetaDataBuilder {
 public:
  // API convenience to get a MetaData reader
  static std::unique_ptr<RowGroupMetaDataBuilder> Make(
      const std::shared_ptr<WriterProperties>& props, const SchemaDescriptor* schema_,
      void* contents);

  ~RowGroupMetaDataBuilder();

  ColumnChunkMetaDataBuilder* NextColumnChunk();
  int num_columns();
  int64_t num_rows();
  int current_column() const;

  void set_num_rows(int64_t num_rows);

  // commit the metadata
  void Finish(int64_t total_bytes_written);

 private:
  explicit RowGroupMetaDataBuilder(const std::shared_ptr<WriterProperties>& props,
                                   const SchemaDescriptor* schema_, void* contents);
  // PIMPL Idiom
  class RowGroupMetaDataBuilderImpl;
  std::unique_ptr<RowGroupMetaDataBuilderImpl> impl_;
};

class PARQUET_EXPORT FileMetaDataBuilder {
 public:
  // API convenience to get a MetaData reader
  static std::unique_ptr<FileMetaDataBuilder> Make(
      const SchemaDescriptor* schema, const std::shared_ptr<WriterProperties>& props,
      const std::shared_ptr<const KeyValueMetadata>& key_value_metadata = NULLPTR);

  ~FileMetaDataBuilder();

  // The prior RowGroupMetaDataBuilder (if any) is destroyed
  RowGroupMetaDataBuilder* AppendRowGroup();

  // Complete the Thrift structure
  std::unique_ptr<FileMetaData> Finish();

 private:
  explicit FileMetaDataBuilder(
      const SchemaDescriptor* schema, const std::shared_ptr<WriterProperties>& props,
      const std::shared_ptr<const KeyValueMetadata>& key_value_metadata = NULLPTR);
  // PIMPL Idiom
  class FileMetaDataBuilderImpl;
  std::unique_ptr<FileMetaDataBuilderImpl> impl_;
};

PARQUET_EXPORT std::string ParquetVersionToString(ParquetVersion::type ver);

}  // namespace parquet

#endif  // PARQUET_FILE_METADATA_H
