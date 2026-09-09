/* {{prefix_lc}}: frame-difference motion detector on the esp_video (V4L2) stack.
 * Previous grayscale frame lives in a static buffer (19 KB) — no PSRAM needed.
 *
 * The capture loop is plain V4L2: request buffers, mmap them once, then
 * DQBUF -> diff -> QBUF forever. Buffers are mapped ONCE at setup rather than
 * per frame; mmap per iteration is the classic way to make a capture loop
 * allocate itself to death. */
/* The luma plane we diff is NEVER larger than the nominal QQVGA — a sensor
 * that only offers a bigger frame is sub-sampled down to this budget. */
static uint8_t s_{{prefix_lc}}_prev[{{prefix}}_CAM_W * {{prefix}}_CAM_H];
static bool s_{{prefix_lc}}_have_prev = false;
static int s_{{prefix_lc}}_fd = -1;
static uint8_t *s_{{prefix_lc}}_buf[{{prefix}}_CAM_BUFS];
static size_t s_{{prefix_lc}}_buf_len[{{prefix}}_CAM_BUFS];
/* What the sensor actually agreed to stream — the diff loop adapts to it. */
static uint32_t s_{{prefix_lc}}_fmt = 0;   /* V4L2_PIX_FMT_* */
static int s_{{prefix_lc}}_w = 0, s_{{prefix_lc}}_h = 0;
static int s_{{prefix_lc}}_step = 1;      /* sub-sample stride, both axes */
static int s_{{prefix_lc}}_dw = 0, s_{{prefix_lc}}_dh = 0;  /* diffed size */

/* Luma of pixel (x, y) in a frame of the negotiated format. Every format the
 * esp_video sensors emit carries luma directly (GREY, YUYV/UYVY) or is a
 * cheap approximation away (RGB565: green channel). */
static inline int {{prefix_lc}}_luma(const uint8_t *f, int x, int y)
{
    switch (s_{{prefix_lc}}_fmt) {
    case V4L2_PIX_FMT_GREY:   return f[y * s_{{prefix_lc}}_w + x];
    case V4L2_PIX_FMT_YUYV:   return f[(y * s_{{prefix_lc}}_w + x) * 2];
    case V4L2_PIX_FMT_UYVY:   return f[(y * s_{{prefix_lc}}_w + x) * 2 + 1];
    case V4L2_PIX_FMT_RGB565: {
        const uint8_t *p = f + (y * s_{{prefix_lc}}_w + x) * 2;
        uint16_t v = (uint16_t)(p[0] | (p[1] << 8));
        return (v >> 5) & 0x3F;               /* 6-bit green, ~luma */
    }
    default: return 0;
    }
}

static size_t {{prefix_lc}}_frame_bytes(void)
{
    size_t px = (size_t)s_{{prefix_lc}}_w * (size_t)s_{{prefix_lc}}_h;
    return s_{{prefix_lc}}_fmt == V4L2_PIX_FMT_GREY ? px : px * 2;
}

/* Ask the sensor for QQVGA GREY — the cheapest thing to diff — and take what
 * it offers when it says no. Which formats a sensor has is a per-sensor build
 * option of the esp_video stack, decided by the board, not by this block:
 * refusing to run on anything but GREY is how a motion detector ships that
 * never sees a frame. Returns ESP_OK only when the device is streaming and
 * every buffer is mapped and queued. */
static esp_err_t {{prefix_lc}}_stream_start(void)
{
    s_{{prefix_lc}}_fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    if (s_{{prefix_lc}}_fd < 0) {
        ESP_LOGE(TAG, "{{prefix_lc}}: cannot open %s", ESP_VIDEO_DVP_DEVICE_NAME);
        return ESP_FAIL;
    }

    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = {{prefix}}_CAM_W;
    fmt.fmt.pix.height = {{prefix}}_CAM_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
    if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_S_FMT, &fmt) != 0) {
        /* Not offered. Enumerate what is, prefer a luma-bearing format, and
           let the sensor pick its own size for it. */
        uint32_t pick = 0;
        for (uint32_t i = 0; i < 16 && pick == 0; i++) {
            struct v4l2_fmtdesc d = {};
            d.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            d.index = i;
            if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_ENUM_FMT, &d) != 0) break;
            if (d.pixelformat == V4L2_PIX_FMT_GREY || d.pixelformat == V4L2_PIX_FMT_YUYV ||
                d.pixelformat == V4L2_PIX_FMT_UYVY || d.pixelformat == V4L2_PIX_FMT_RGB565) {
                pick = d.pixelformat;
            }
        }
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_G_FMT, &fmt) != 0) {
            ESP_LOGE(TAG, "{{prefix_lc}}: sensor refused %dx%d GREY and reports no format",
                     {{prefix}}_CAM_W, {{prefix}}_CAM_H);
            return ESP_FAIL;
        }
        if (pick != 0 && pick != fmt.fmt.pix.pixelformat) {
            fmt.fmt.pix.pixelformat = pick;
            if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_S_FMT, &fmt) != 0) {
                (void)ioctl(s_{{prefix_lc}}_fd, VIDIOC_G_FMT, &fmt);
            }
        }
    }
    s_{{prefix_lc}}_fmt = fmt.fmt.pix.pixelformat;
    s_{{prefix_lc}}_w = (int)fmt.fmt.pix.width;
    s_{{prefix_lc}}_h = (int)fmt.fmt.pix.height;
    if (s_{{prefix_lc}}_fmt != V4L2_PIX_FMT_GREY && s_{{prefix_lc}}_fmt != V4L2_PIX_FMT_YUYV &&
        s_{{prefix_lc}}_fmt != V4L2_PIX_FMT_UYVY && s_{{prefix_lc}}_fmt != V4L2_PIX_FMT_RGB565) {
        ESP_LOGE(TAG, "{{prefix_lc}}: sensor streams %c%c%c%c only — no luma to diff",
                 (char)(s_{{prefix_lc}}_fmt), (char)(s_{{prefix_lc}}_fmt >> 8),
                 (char)(s_{{prefix_lc}}_fmt >> 16), (char)(s_{{prefix_lc}}_fmt >> 24));
        return ESP_FAIL;
    }
    /* Sub-sample a larger frame down to the QQVGA diff budget. */
    s_{{prefix_lc}}_step = 1;
    while ((s_{{prefix_lc}}_w / s_{{prefix_lc}}_step) * (s_{{prefix_lc}}_h / s_{{prefix_lc}}_step) >
           {{prefix}}_CAM_W * {{prefix}}_CAM_H) {
        s_{{prefix_lc}}_step++;
    }
    s_{{prefix_lc}}_dw = s_{{prefix_lc}}_w / s_{{prefix_lc}}_step;
    s_{{prefix_lc}}_dh = s_{{prefix_lc}}_h / s_{{prefix_lc}}_step;
    ESP_LOGI(TAG, "{{prefix_lc}}: streaming %dx%d %c%c%c%c, diffing %dx%d (step %d)",
             s_{{prefix_lc}}_w, s_{{prefix_lc}}_h,
             (char)(s_{{prefix_lc}}_fmt), (char)(s_{{prefix_lc}}_fmt >> 8),
             (char)(s_{{prefix_lc}}_fmt >> 16), (char)(s_{{prefix_lc}}_fmt >> 24),
             s_{{prefix_lc}}_dw, s_{{prefix_lc}}_dh, s_{{prefix_lc}}_step);

    struct v4l2_requestbuffers req = {};
    req.count = {{prefix}}_CAM_BUFS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "{{prefix_lc}}: VIDIOC_REQBUFS failed");
        return ESP_FAIL;
    }

    for (uint32_t i = 0; i < req.count && i < {{prefix}}_CAM_BUFS; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "{{prefix_lc}}: VIDIOC_QUERYBUF %u failed", (unsigned)i);
            return ESP_FAIL;
        }
        s_{{prefix_lc}}_buf_len[i] = buf.length;
        s_{{prefix_lc}}_buf[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                MAP_SHARED, s_{{prefix_lc}}_fd, buf.m.offset);
        if (s_{{prefix_lc}}_buf[i] == MAP_FAILED) {
            ESP_LOGE(TAG, "{{prefix_lc}}: mmap of buffer %u failed", (unsigned)i);
            return ESP_FAIL;
        }
        if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "{{prefix_lc}}: initial VIDIOC_QBUF %u failed", (unsigned)i);
            return ESP_FAIL;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "{{prefix_lc}}: VIDIOC_STREAMON failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void {{prefix_lc}}_motion_task(void *arg)
{
    (void)arg;
    bool motion = false;
    int64_t last_motion_ms = 0;
    const int total = s_{{prefix_lc}}_dw * s_{{prefix_lc}}_dh;
    const int area_px = (total * {{prefix}}_CAM_AREA_PCT) / 100;
    const size_t need = {{prefix_lc}}_frame_bytes();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CAM_POLL_MS));

        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGW(TAG, "{{prefix_lc}}: frame capture failed");
            continue;
        }

        if (buf.index < {{prefix}}_CAM_BUFS && buf.bytesused >= need) {
            const uint8_t *frame = s_{{prefix_lc}}_buf[buf.index];
            int changed = 0;
            int i = 0;
            for (int y = 0; y < s_{{prefix_lc}}_dh; y++) {
                for (int x = 0; x < s_{{prefix_lc}}_dw; x++, i++) {
                    int l = {{prefix_lc}}_luma(frame, x * s_{{prefix_lc}}_step, y * s_{{prefix_lc}}_step);
                    if (s_{{prefix_lc}}_have_prev) {
                        int d = l - (int)s_{{prefix_lc}}_prev[i];
                        if (d < 0) d = -d;
                        if (d > {{prefix}}_CAM_PX_THRESH) changed++;
                    }
                    s_{{prefix_lc}}_prev[i] = (uint8_t)l;
                }
            }
            if (s_{{prefix_lc}}_have_prev) {
                int64_t now_ms = (int64_t)(esp_timer_get_time() / 1000);
                if (changed > area_px) last_motion_ms = now_ms;
                bool now_motion = (now_ms - last_motion_ms) < {{prefix}}_CAM_HOLD_MS && last_motion_ms != 0;
                if (now_motion != motion) {
                    motion = now_motion;
                    ESP_LOGI(TAG, "{{prefix_lc}}: motion %s (%d px changed)",
                             motion ? "detected" : "cleared", changed);
                    zc_vision_emit(0, motion ? 1 : 0);
                }
            }
            s_{{prefix_lc}}_have_prev = true;
        }

        /* Hand the buffer back even on a short frame — skipping this starves
           the driver of buffers and the next DQBUF blocks forever. */
        if (ioctl(s_{{prefix_lc}}_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGW(TAG, "{{prefix_lc}}: VIDIOC_QBUF failed; capture may stall");
        }
    }
}
