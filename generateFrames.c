#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <sys/time.h>

#define OUTPUT_FILE "frames"
#define TARGET_WIDTH 120
#define TARGET_HEIGHT 30

void writeToFile(const char *contents) {
    FILE *f = fopen(OUTPUT_FILE, "a");
    if (f == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }
    fprintf(f, "%s", contents);
    fclose(f);
}

void wipeFile() {
    FILE *f = fopen(OUTPUT_FILE, "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_video>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];

    // Prompt the user to enter the desired FPS
    int desired_fps;
    printf("Enter the desired FPS: ");
    scanf("%d", &desired_fps);

    wipeFile();

    // Initialize FFmpeg
    avformat_network_init();

    // Open the input file
    AVFormatContext *format_context = NULL;
    if (avformat_open_input(&format_context, input_file, NULL, NULL) != 0) {
        printf("Error: Couldn't open input file\n");
        return 1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(format_context, NULL) < 0) {
        printf("Error: Couldn't find stream information\n");
        return 1;
    }

    // Find the first video stream
    int video_stream_index = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        printf("Error: Couldn't find video stream\n");
        return 1;
    }

    // Get a pointer to the codec context for the video stream
    AVCodecParameters *codec_parameters = format_context->streams[video_stream_index]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if (codec == NULL) {
        printf("Error: Unsupported codec\n");
        return 1;
    }
    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (codec_context == NULL) {
        printf("Error: Couldn't allocate codec context\n");
        return 1;
    }
    if (avcodec_parameters_to_context(codec_context, codec_parameters) < 0) {
        printf("Error: Couldn't copy codec parameters to codec context\n");
        return 1;
    }
    if (avcodec_open2(codec_context, codec, NULL) < 0) {
        printf("Error: Couldn't open codec\n");
        return 1;
    }

    // Allocate video frame and initialize swscale context
    AVFrame *frame = av_frame_alloc();
    AVFrame *frame_rescaled = av_frame_alloc();
    if (frame == NULL || frame_rescaled == NULL) {
        printf("Error: Couldn't allocate frame\n");
        return 1;
    }

    // Allocate buffer for rescaled frame
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, TARGET_WIDTH, TARGET_HEIGHT, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
    av_image_fill_arrays(frame_rescaled->data, frame_rescaled->linesize, buffer, AV_PIX_FMT_RGB24, TARGET_WIDTH, TARGET_HEIGHT, 1);

    struct SwsContext *sws_context = sws_getContext(
        codec_context->width, codec_context->height, codec_context->pix_fmt,
        TARGET_WIDTH, TARGET_HEIGHT, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    if (sws_context == NULL) {
        printf("Error: Couldn't initialize swscale context\n");
        return 1;
    }

    // Calculate frame rate adjustment factor
    double frame_rate = av_q2d(format_context->streams[video_stream_index]->avg_frame_rate);
    double factor = frame_rate / desired_fps;

    // Get total number of frames
    int total_frames = format_context->streams[video_stream_index]->nb_frames;
    if (total_frames == 0) {
        total_frames = (int)(format_context->duration / AV_TIME_BASE * frame_rate);
    }

    // Start time for calculating ETA
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // Read frames from the video file
    AVPacket packet;
    int frame_finished;
    int frame_counter = 0;
    while (av_read_frame(format_context, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            avcodec_send_packet(codec_context, &packet);
            avcodec_receive_frame(codec_context, frame);

            // Resize frame to target dimensions
            sws_scale(sws_context, frame->data, frame->linesize, 0, codec_context->height, frame_rescaled->data, frame_rescaled->linesize);

            // Process frame based on adjustment factor
            if ((frame_counter % (int)factor) == 0) {
                // Loop through each row (height)
                for (int y = 0; y < TARGET_HEIGHT; y++) {
                    // Loop through each column (width)
                    for (int x = 0; x < TARGET_WIDTH; x++) {
                        // Get the pixel value at position (x, y)
                        uint8_t *pixel_data = frame_rescaled->data[0] + y * frame_rescaled->linesize[0] + x * 3;
                        // Check if pixel is dark
                        if (pixel_data[0] < 128 && pixel_data[1] < 128 && pixel_data[2] < 128) {
                            writeToFile(" ");
                        } else {
                            writeToFile("#");
                        }
                    }
                    writeToFile("\n");
                }

                // Update progress bar
                float progress = (float)frame_counter / total_frames;
                int bar_length = 50;
                int num_symbols = progress * bar_length;
                struct timeval current_time;
                gettimeofday(&current_time, NULL);
                float elapsed_time = (current_time.tv_sec - start_time.tv_sec) + (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
                float eta = elapsed_time / progress - elapsed_time;
                printf("\rProgress: [");
                for (int i = 0; i < bar_length; i++) {
                    if (i < num_symbols) {
                        printf("=");
                    } else {
                        printf(" ");
                    }
                }
                printf("] %.2f%% ETA: %.2fs    ", progress * 100, eta);
                fflush(stdout);
            }
            frame_counter++;
        }
        av_packet_unref(&packet);
    }

    printf("\n");

    // Free resources
    av_frame_free(&frame);
    av_frame_free(&frame_rescaled);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    sws_freeContext(sws_context);
    av_free(buffer);

    fflush(stdout);
    printf("Done!\n");

    return 0;
}
