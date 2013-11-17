#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include <GL/gl.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/progress.hpp>
#include <gflags/gflags.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "recordio.h"
#include "window.h"

DEFINE_string(image_directory, "",
              "Base directory for images, we recursively search for all "
              "jpegs in this directory and sub-directories");
DEFINE_string(directory_blacklist, "",
              "Comma seperated list of directories to ignore.");

DEFINE_bool(generate_thumbnails, true,
            "Generate small versions of all images, stored in icon_file.");
DEFINE_string(thumbnail_file, "thumbnails.bin",
              "File for caching small versions of all images.");

DEFINE_string(single_image, "",
              "If set, only generate the mosaic for this image.");

using boost::filesystem::directory_iterator;
using boost::filesystem::is_directory;
using boost::filesystem::path;

struct Thumbnail {
  char filename[256];
  uint8_t pixels[3 * 20 * 15];
};

class ThumbnailLibrary {
 public:
  ThumbnailLibrary() {
  }

  void Add(const Thumbnail& thumbnail) {
    thumbnails_.push_back(thumbnail);
  }

  void Write(const std::string& filename) const {
    std::ofstream output(filename);
    file::RecordWriter record_writer(&output);
    for (const Thumbnail& thumbnail : thumbnails_) {
      record_writer.Write<Thumbnail>(thumbnail);
    }
    record_writer.Close();
  }
  
  void Read(const std::string& filename) {
    std::ifstream input(filename);
    file::RecordReader record_reader(&input);
    thumbnails_.clear();
    thumbnails_.push_back(Thumbnail());
    while (record_reader.Read<Thumbnail>(&thumbnails_.back())) {
      thumbnails_.push_back(Thumbnail());
    }
    thumbnails_.pop_back();
    record_reader.Close();

    std::cout << "Loaded " << thumbnails_.size() << " thumbnails." << std::endl;
  }

  const Thumbnail* FindClosest(const uint8_t* pixels) const {
    const Thumbnail* best = nullptr;
    int best_diff = std::numeric_limits<int>::max();
    for (const Thumbnail& thumbnail : thumbnails_) {
      int diff = 0;
      for (int i = 0; i < 3 * 20 * 15; ++i) {
        diff += (pixels[i] - thumbnail.pixels[i]) *
            (pixels[i] - thumbnail.pixels[i]);
      }
      if (diff < best_diff) {
        best_diff = diff;
        best = &thumbnail;
      }
    }
    return best;
  }

 private:
  std::vector<Thumbnail> thumbnails_;
};

class Mosaic {
 public:
  Mosaic(const cv::Mat& original,
         const ThumbnailLibrary* library) : library_(library) {
    Build(original);
  }

  void Draw() const {
    glPixelZoom(0.5, 0.5);
    for (int r = 0; r < 80; ++r) {
      for (int c = 0; c < 80; ++c) {
        glRasterPos2f(0.5 * 20 * c, 0.5 * 15 * r);
        const Thumbnail* thumbnail = mosaic_[r * 80 + c];
        glDrawPixels(20, 15, GL_BGR, GL_UNSIGNED_BYTE,
                     thumbnail->pixels);
      }
    }
  }

 private:
  void Build(const cv::Mat& original) {
    uint8_t pixels[3 * 20 * 15];
    for (int r = 0; r < 80; ++r) {
      for (int c = 0; c < 80; ++c) {
        for (int y = 0; y < 15; ++y) {
          for (int x = 0; x < 20; ++x) {
            int orig_y = r * 15 + y;
            int orig_x = c * 20 + x;
            pixels[3 * (20 * y + x) + 0] =
                original.data[3 * (1600 * orig_y + orig_x) + 0];
            pixels[3 * (20 * y + x) + 1] =
                original.data[3 * (1600 * orig_y + orig_x) + 1];
            pixels[3 * (20 * y + x) + 2] =
                original.data[3 * (1600 * orig_y + orig_x) + 2];
          }
        }
        mosaic_.push_back(library_->FindClosest(pixels));
      }
    }
  }

  const ThumbnailLibrary* library_;
  std::vector<const Thumbnail*> mosaic_;
};

class MosaicWindow : public graphics::Window2d {
 public:
  MosaicWindow() : graphics::Window2d(800, 600, "Infinipic") {}
  virtual ~MosaicWindow() {}

  void SetMosaic(const Mosaic* mosaic) {
    mosaic_ = mosaic;
  }
  
 protected:
  virtual void Keypress(unsigned int key) {
    switch (key) {
      case XK_Escape:
        Close();
        break;
    }
  }

  virtual void Draw() {
    glClear(GL_COLOR_BUFFER_BIT);
    mosaic_->Draw();
  }

 private:
  const Mosaic* mosaic_;
};

std::set<std::string> Split(const std::string& str, const char delim) {
  std::stringstream ss(str);
  std::set<std::string> result;
  std::string temp;
  while (std::getline(ss, temp, delim)) {
    result.insert(temp);
  }
  return result;
}

// Recursively gather all photo paths in the given directory, where a photo
// is anything that ends in .jpg or .jpeg.
void GatherPhotos(const path& dir_path,
                  std::vector<std::string>* photos) {
  static std::set<std::string> blacklist(Split(FLAGS_directory_blacklist, ','));
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(dir_path);
       itr != end_itr; ++itr) {
    if (is_directory(itr->status()) &&
        blacklist.count(itr->path().string()) == 0) {
      GatherPhotos(itr->path(), photos);
    } else {
      const std::string& file_path = itr->path().string();
      if (boost::algorithm::ends_with(file_path, ".jpg") ||
          boost::algorithm::ends_with(file_path, ".jpeg")) {
        photos->push_back(file_path);
      }
    }
  }
}

void GenerateThumbnails(const std::string& output_path) {
  std::vector<std::string> photos;
  GatherPhotos(path(FLAGS_image_directory), &photos);

  ThumbnailLibrary library;
  boost::progress_display progress_bar(photos.size(), std::cout,
                                       "Generating thumbnails...\n");
  for (const std::string& photo : photos) {
    cv::Mat image = cv::imread(photo, CV_LOAD_IMAGE_COLOR);
    if (image.cols * 6 == image.rows * 8) {
      cv::resize(image, image, cv::Size(20, 15));
      cv::flip(image, image, 0);
      Thumbnail thumbnail;
      strncpy(thumbnail.filename, photo.c_str(), 255);
      memcpy(thumbnail.pixels, image.data, 3 * 20 * 15);
      thumbnail.filename[255] = 0;
      library.Add(thumbnail);
    }
    ++progress_bar;
  }
  
  library.Write(output_path);
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  
  if (FLAGS_generate_thumbnails) {
    GenerateThumbnails(FLAGS_thumbnail_file);
  }

  ThumbnailLibrary library;
  library.Read(FLAGS_thumbnail_file);

  if (!FLAGS_single_image.empty()) {
    cv::Mat image = cv::imread(FLAGS_single_image, CV_LOAD_IMAGE_COLOR);
    cv::resize(image, image, cv::Size(1600,1200));
    cv::flip(image, image, 0);

    Mosaic mosaic(image, &library);
  
    MosaicWindow window;
    window.SetMosaic(&mosaic);
    window.Run();
  }
  
  return 0;
}
