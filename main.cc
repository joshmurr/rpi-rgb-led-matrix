#include "thread.h"
#include "led-matrix.h"

#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

// Base-class for a Thread that does something with a matrix.
class RGBMatrixManipulator : public Thread {
public:
  RGBMatrixManipulator(RGBMatrix *m) : running_(true), matrix_(m) {}
  virtual ~RGBMatrixManipulator() { running_ = false; }

  // Run() implementation needs to check running_ regularly.

protected:
  volatile bool running_;  // TODO: use mutex, but this is good enough for now.
  RGBMatrix *const matrix_;
};

// Pump pixels to screen. Needs to be high priority real-time because jitter
// here will make the PWM uneven.
class DisplayUpdater : public RGBMatrixManipulator {
public:
  DisplayUpdater(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  void Run() {
    while (running_) {
      matrix_->UpdateScreen();
    }
  }
};

// -- The following are demo image generators.

// Simple generator that pulses through RGB and White.
class ColorPulseGenerator : public RGBMatrixManipulator {
public:
  ColorPulseGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int columns = matrix_->columns();
    uint32_t count = 0;
    while (running_) {
      usleep(5000);
      ++count;
      int color = (count >> 9) % 6;
      int value = count & 0xFF;
      if (count & 0x100) value = 255 - value;
      int r, g, b;
      switch (color) {
      case 0: r = value; g = b = 0; break;
      case 1: r = g = value; b = 0; break;
      case 2: g = value; r = b = 0; break;
      case 3: g = b = value; r = 0; break;
      case 4: b = value; r = g = 0; break;
      default: r = g = b = value; break;
      }
      for (int x = 0; x < columns; ++x)
        for (int y = 0; y < 32; ++y)
          matrix_->SetPixel(x, y, r, g, b);
    }
  }
};

// Simple class that generates a rotating block on the screen.
class RotatingBlockGenerator : public RGBMatrixManipulator {
public:
  RotatingBlockGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  uint8_t scale_col(int val, int lo, int hi) {
    if (val < lo) return 0;
    if (val > hi) return 255;
    return 255 * (val - lo) / (hi - lo);
  }

  void Run() {
    const float deg_to_rad = 2 * 3.14159265 / 360;
    int rotation = 0;
    while (running_) {
      ++rotation;
      usleep(15 * 1000);
      rotation %= 360;
      for (int x = -8; x < 40; ++x) {
        for (int y = -8; y < 40; ++y) {
          float disp_x, disp_y;
          Rotate(x - 16, y - 16, deg_to_rad * rotation, &disp_x, &disp_y);
          if (x >= 5 && x < 28 && y >= 5 && y < 28) {
              matrix_->SetPixel(disp_x + 16, disp_y + 16,
                                scale_col(x, 5, 27),
                                255 - scale_col(y, 5, 27),
                                scale_col(y, 5, 27));
            } else if (x == 16 && y == 3) {
              matrix_->SetPixel(disp_x + 16, disp_y + 16, 255, 0, 0);
            } else if (x == 3 && y == 16) {
              matrix_->SetPixel(disp_x + 16, disp_y + 16, 0, 255, 0);
            } else if (x == 29 && y == 16) {
              matrix_->SetPixel(disp_x + 16, disp_y + 16, 0, 0, 255);
            } else {
              matrix_->SetPixel(disp_x + 16, disp_y + 16, 0, 0, 0);
            }
        }
      }
    }
  }

private:
  void Rotate(int x, int y, float angle,
              float *new_x, float *new_y) {
    *new_x = x * cosf(angle) - y * sinf(angle);
    *new_y = x * sinf(angle) + y * cosf(angle);
  }
};

class Blend : public RGBMatrixManipulator {
  public:
    Blend(RGBMatrix *m) : RGBMatrixManipulator(m) {}

    const int columns = matrix_->columns();
    const int rows = matrix_->rows();
    const float deg_to_rad = 2 * 3.14159265 / 360;

    void Run() {
      int r, g, b;
      float count = 0.0;
      while(running_){
        count += 0.1;
        usleep(50000);
        for (int x = 0; x < columns; ++x){
          for (int y = 0; y < rows; ++y){
            float u = 0.25 + sinf(count + (float)x / 32) * 0.25;
            float v = 0.25 + cosf(count + (float)y / 32) * 0.25;
            float s = 1.0 - v - u;
            r = u * 64.0;
            g = v * 64.0;
            b = s * 64.0;
            matrix_->SetPixel(x, y, r, g, b);
          }
        }
      }
    }

  private:
};

class Flame : public RGBMatrixManipulator {
  public:
    const int columns = matrix_->columns();
    const int rows = matrix_->rows();
    int buf[32 * 32];

		const int colours[32] = {
				0x020202,
				0x030303,
				0x2F0F07,
				0x470F07,
				0x571707,
				0x671F07,
				0x771F07,
				0x8F2707,
				0x9F2F07,
				0xAF3F07,
				0xBF4707,
				0xC74707,
				0xDF4F07,
				0xDF5707,
				0xDF5707,
				0xD75F07,
				0xD7670F,
				0xCF6F0F,
				0xCF770F,
				0xCF7F0F,
				0xCF8717,
				0xC78717,
				0xC7971F,
				0xBF9F1F,
				0xBFA727,
				0xBFAF2F,
				0xB7AF2F,
				0xB7B737,
				0xCFCF6F,
				0xDFDF9F,
				0xEFEFC7,
				0xFFFFFF
		};

    Flame(RGBMatrix *m) : RGBMatrixManipulator(m) {
			// Set up canvas
      for(int x=0; x<32; ++x){
        for(int y=0; y<32; ++y){
          int idx = y * 32 + x;
          if(y == 31) buf[idx] = 31;
          else buf[idx] = 0;
        }
      }
		}


    void Run(){
      uint8_t r, g, b;
      while(running_){
        usleep(50000);
        DoFire();
        for (int x = 0; x < columns; ++x){
          for (int y = 0; y < rows; ++y){
						int colour_idx = buf[y * columns + x];
            int colour = colours[colour_idx];
            r = (colour >> 16) & 0xFF;
            b = (colour >> 8) & 0xFF;
            g = colour & 0xFF;
            matrix_->SetPixel(x, y, r, g, b);
          }
        }
      }
    }

  private:
    void DoFire(void){
      for (int x = 0; x < columns; ++x){
        for (int y = 1; y < rows; ++y){
          SpreadFire(y * 32 + x);
        }
      }
    }
    void SpreadFire(int from){
			int p = buf[from];
      int to = from - columns;
      if(p < 1) buf[to] = 0;
      else {
        int r = (rand() % 5);
        buf[to] = p - (r);
      }
//std::cout << buf[to] << std::endl;
    }

		/*
		void AutoColours(void){
      uint8_t r, g, b;
      for(int i=0; i<32; ++i) {
        r = i << 3;
        g = 32 - i;
        b = i << 2;
        colours[i] = (r << 16) | (g << 8) | b;
      }
    }
		*/
};

class ImageScroller : public RGBMatrixManipulator {
public:
  ImageScroller(RGBMatrix *m)
    : RGBMatrixManipulator(m), image_(NULL), horizontal_position_(0) {
  }

  // _very_ simplified. Can only read binary P6 PPM. Expects newlines in headers
  // Not really robust. Use at your own risk :)
  bool LoadPPM(const char *filename) {
    if (image_) {
      delete [] image_;
      image_ = NULL;
    }
    FILE *f = fopen(filename, "r");
    if (f == NULL) return false;
    char header_buf[256];
    const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
      fclose(f); return false; }
    if (sscanf(line, "P6 ") == EOF)
      EXIT_WITH_MSG("Can only handle P6 as PPM type.");
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d %d ", &width_, &height_) != 2)
      EXIT_WITH_MSG("Width/height expected");
    int value;
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
      EXIT_WITH_MSG("Only 255 for maxval allowed.");
    const size_t pixel_count = width_ * height_;
    image_ = new Pixel [ pixel_count ];
    assert(sizeof(Pixel) == 3);   // we make that assumption.
    if (fread(image_, sizeof(Pixel), pixel_count, f) != pixel_count) {
      line = "";
      EXIT_WITH_MSG("Not enough pixels read.");
    }
#undef EXIT_WITH_MSG
    fclose(f);
    fprintf(stderr, "Read image with %dx%d\n", width_, height_);
    horizontal_position_ = 0;
    return true;
  }

  void Run() {
    const int columns = matrix_->columns();
    while (running_) {
      if (image_ == NULL) {
        usleep(100 * 1000);
        continue;
      }
      usleep(30 * 1000);
      for (int x = 0; x < columns; ++x) {
        for (int y = 0; y < 32; ++y) {
          const Pixel &p = getPixel((horizontal_position_ + x) % width_, y);
          // Display upside down on my desk. Lets flip :)
          int disp_x = columns - x;
          int disp_y = 31 - y;
          matrix_->SetPixel(disp_x, disp_y, p.red, p.green, p.blue);
        }
      }
      ++horizontal_position_;
    }
  }

private:
  struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  // Read line, skip comments.
  char *ReadLine(FILE *f, char *buffer, size_t len) {
    char *result;
    do {
      result = fgets(buffer, len, f);
    } while (result != NULL && result[0] == '#');
    return result;
  }

  const Pixel &getPixel(int x, int y) {
    static Pixel dummy;
    if (x < 0 || x > width_ || y < 0 || y > height_) return dummy;
    return image_[x + width_ * y];
  }

  int width_;
  int height_;
  Pixel *image_;
  uint32_t horizontal_position_;
};

int main(int argc, char *argv[]) {
  int demo = 0;
  if (argc > 1) {
    demo = atoi(argv[1]);
  }
  fprintf(stderr, "Using demo %d\n", demo);

  GPIO io;
  if (!io.Init())
    return 1;

  RGBMatrix m(&io);
    
  RGBMatrixManipulator *image_gen = NULL;
  switch (demo) {
  case 0:
    image_gen = new RotatingBlockGenerator(&m);
    break;

  case 1:
    if (argc > 2) {
      ImageScroller *scroller = new ImageScroller(&m);
      if (!scroller->LoadPPM(argv[2]))
        return 1;
      image_gen = scroller;
    } else {
      fprintf(stderr, "Demo %d Requires PPM image as parameter", demo);
      return 1;
    }
    break;

  case 2:
    image_gen = new Blend(&m);
    break;

  case 3:
    image_gen = new Flame(&m);
    break;

  default:
    image_gen = new ColorPulseGenerator(&m);
    break;
  }

  if (image_gen == NULL)
    return 1;

  RGBMatrixManipulator *updater = new DisplayUpdater(&m);
  updater->Start(10);  // high priority

  image_gen->Start();

  // Things are set up. Just wait for <RETURN> to be pressed.
  printf("Press <RETURN> to exit and reset LEDs\n");
  getchar();

  // Stopping threads and wait for them to join.
  delete image_gen;
  delete updater;

  // Final thing before exit: clear screen and update once, so that
  // we don't have random pixels burn
  m.ClearScreen();
  m.UpdateScreen();

  return 0;
}
