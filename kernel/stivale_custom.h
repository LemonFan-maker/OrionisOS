#pragma once
#include <stdint.h>

// 将结构体名称从 stivale_header 改为 stivale_header_tag 以避免冲突
struct stivale_header_tag {
  uint64_t stack;
  uint16_t flags;
  uint16_t framebuffer_width;
  uint16_t framebuffer_height;
  uint16_t framebuffer_bpp;
  uint64_t entry_point;
} __attribute__((packed));