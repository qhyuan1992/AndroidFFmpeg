//
// Created by wlanjie on 16/5/3.
//

#include "filter.h"
#include "log.h"

int configure_input_video_filter(FilterGraph *fg, AVFilterInOut *in) {
    int ret = 0;
    AVFilter *buffer = avfilter_get_by_name("buffer");
    if (!buffer) {
        return AVERROR(EINVAL);
    }
    AVBPrint args;
    av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
    InputFilter *ifilter = fg->input;
    const int width = ifilter->ist->dec_ctx->width;
    const int height = ifilter->ist->dec_ctx->height;
    const enum AVPixelFormat formt = ifilter->ist->dec_ctx->pix_fmt;
    const struct AVRational time_base = ifilter->ist->dec_ctx->time_base;
    const struct AVRational sample_aspect_ratio = ifilter->ist->dec_ctx->sample_aspect_ratio;
    av_bprintf(&args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d", width, height, formt,
               time_base.num, time_base.den, sample_aspect_ratio.num, sample_aspect_ratio.den);
    AVRational fr = av_guess_frame_rate(input_file->ic, ifilter->ist->st, NULL);
    if (fr.num && fr.den) {
        av_bprintf(&args, ":frame_rate=%d/%d", fr.num, fr.den);
    }
    char name[255];
    snprintf(name, sizeof(name), "video graph input stream %d", ifilter->ist->st->index);
    ret = avfilter_graph_create_filter(&fg->input->filter, buffer, name, args.str, NULL, fg->graph);
    if (ret < 0) {
        return ret;
    }
    AVFilterContext *last_filter = fg->input->filter;
    double theta = get_rotation(fg->input->ist->st);
    if (fabs(theta - 90) < 1.0) {
        AVFilterContext *transpose_context;
        const AVFilter *transpose = avfilter_get_by_name("transpose");
        ret = avfilter_graph_create_filter(&transpose_context, transpose, "transpose", "clock", NULL, fg->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(last_filter, 0, transpose_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        last_filter = transpose_context;
    } else if (fabs(theta - 180) < 1.0) {
        AVFilterContext *hflip_context;
        ret = avfilter_graph_create_filter(&hflip_context, avfilter_get_by_name("hflip"), "hflip", NULL, NULL, fg->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(last_filter, 0, hflip_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        last_filter = hflip_context;
        AVFilterContext *vflip_context;
        ret = avfilter_graph_create_filter(&vflip_context, avfilter_get_by_name("vflip"), "vflip", NULL, NULL, fg->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(last_filter, 0, vflip_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        last_filter = vflip_context;
    } else if (fabs(theta - 270) < 1.0) {
        AVFilterContext *transpose_context;
        ret = avfilter_graph_create_filter(&transpose_context, avfilter_get_by_name("transpose"), "transpose", "cclock", NULL, fg->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(last_filter, 0, transpose_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        last_filter = transpose_context;
    } else if (fabs(theta) > 1.0) {
        char rotate_buf[64];
        snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
        AVFilterContext *rotate_context;
        ret = avfilter_graph_create_filter(&rotate_context, avfilter_get_by_name("rotate"), "rotate", rotate_buf, NULL, fg->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(last_filter, 0, rotate_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        last_filter = rotate_context;
    }

    ret = avfilter_link(last_filter, 0, in->filter_ctx, 0);
    if (ret < 0) {
        return ret;
    }
    return ret;
}

int configure_input_audio_filter(FilterGraph *graph, AVFilterInOut *in) {
    int ret = 0;
    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    if (!abuffer) {
        return AVERROR(EAGAIN);
    }
    AVBPrint args;
    av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_bprintf(&args, "time_base=%d/%d:sample_rate=%d:sample_fmt=%s", 1, graph->input->ist->dec_ctx->sample_rate,
               graph->input->ist->dec_ctx->sample_rate, av_get_sample_fmt_name(graph->input->ist->dec_ctx->sample_fmt));
    if (graph->input->ist->dec_ctx->channel_layout) {
        av_bprintf(&args, ":channel_layout=0x%"PRIX64, graph->input->ist->dec_ctx->channel_layout);
    } else {
        av_bprintf(&args, ":channels=%d", graph->input->ist->dec_ctx->channels);
    }
    char name[255];
    snprintf(name, sizeof(name), "audio graph input stream %d", graph->input->ist->st->index);
    ret = avfilter_graph_create_filter(&graph->input->filter, abuffer, name, args.str, NULL, graph->graph);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    ret = avfilter_link(graph->input->filter, 0, in->filter_ctx, 0);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    return ret;
}

int configure_output_video_filter(FilterGraph *graph, AVFilterInOut *out) {
    int ret = 0;
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    char name[255];
    snprintf(name, sizeof(name), "video graph output stream %d", graph->output->ost->st->index);
    ret = avfilter_graph_create_filter(&graph->output->filter, buffersink, name, NULL, NULL, graph->graph);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    AVFilterContext *format_context;
    const AVFilter *format_filter = avfilter_get_by_name("format");
    const enum AVPixelFormat *p = graph->output->ost->enc->pix_fmts;
    AVIOContext *s;
    ret = avio_open_dyn_buf(&s);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    for (; *p != AV_PIX_FMT_NONE; p++) {
        avio_printf(s, "%s|", av_get_pix_fmt_name(*p));
    }
    uint8_t *tmp;
    int len = avio_close_dyn_buf(s, &tmp);
    tmp[len - 1] = 0;
    ret = avfilter_graph_create_filter(&format_context, format_filter, "format", (char *) tmp, NULL, graph->graph);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    ret = avfilter_link(out->filter_ctx, 0, format_context, 0);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    AVFilterContext *scale_context = NULL;
    if (graph->output->ost->enc_ctx->width > 0 && graph->output->ost->enc_ctx->height > 0) {
        AVFilter *scale_filter = avfilter_get_by_name("scale");
        AVBPrint args;
        av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
        av_bprintf(&args, "%d:%d", graph->output->ost->enc_ctx->width, graph->output->ost->enc_ctx->height);
        ret = avfilter_graph_create_filter(&scale_context, scale_filter, "scale", args.str, NULL, graph->graph);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
        ret = avfilter_link(format_context, 0, scale_context, 0);
        if (ret < 0) {
            av_err2str(ret);
            return ret;
        }
    }
    ret = avfilter_link(scale_context ? scale_context : format_context, 0, graph->output->filter, 0);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    return ret;
}

#define DEF_CHOOSE_FORMAT(type, var, supported_list, none, get_name)                                            \
char *choose_ ## var ## s(OutputStream *ost) {                                                                  \
    if (ost->enc_ctx->var != none) {                                                                            \
        get_name(ost->enc_ctx->var);                                                                            \
        return av_strdup(name);                                                                                 \
    } else if (ost->enc->supported_list) {                                                                      \
        const type *p;                                                                                          \
        AVIOContext *s = NULL;                                                                                  \
        uint8_t *tmp = NULL;                                                                                    \
        if (avio_open_dyn_buf(&s) < 0) {                                                                        \
            return NULL;                                                                                        \
        }                                                                                                       \
        for (p = ost->enc->supported_list; *p != none; p++) {                                                   \
            get_name(*p);                                                                                       \
            avio_printf(s, "%s|", name);                                                                        \
        }                                                                                                       \
        int len = avio_close_dyn_buf(s, &tmp);                                                                  \
        tmp[len - 1] = 0;                                                                                       \
        return (char *) tmp;                                                                                    \
    } else {                                                                                                    \
        return NULL;                                                                                            \
    }                                                                                                           \
}                                                                                                               \

#define GET_SAMPLE_FMT_NAME(sample_fmt)                                                                         \
    const char *name = av_get_sample_fmt_name(sample_fmt);                                                      \

#define GET_SAMPLE_RATE_NAME(sample_rate)                                                                       \
    char name[255];                                                                                             \
    snprintf(name, sizeof(name), "%d", sample_rate);                                                            \

#define GET_CHANNEL_LAYOUT_NAME(channel_layout)                                                                 \
    char name[255];                                                                                             \
    snprintf(name, sizeof(name), "0x%"PRIX64, channel_layout);                                                  \

DEF_CHOOSE_FORMAT(enum AVSampleFormat, sample_fmt, sample_fmts, AV_SAMPLE_FMT_NONE, GET_SAMPLE_FMT_NAME);

DEF_CHOOSE_FORMAT(int, sample_rate, supported_samplerates, 0, GET_SAMPLE_RATE_NAME);

DEF_CHOOSE_FORMAT(uint64_t, channel_layout, channel_layouts, 0, GET_CHANNEL_LAYOUT_NAME);

int configure_output_audio_filter(FilterGraph *graph, AVFilterInOut *out) {
    int ret = 0;
    AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    char name[255];
    snprintf(name, sizeof(name), "audio graph output stream %d", graph->output->ost->st->index);
    ret = avfilter_graph_create_filter(&graph->output->filter, abuffersink, name, NULL, NULL, graph->graph);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    const char *sample_fmts = choose_sample_fmts(graph->output->ost);
    const char *sample_rates = choose_sample_rates(graph->output->ost);
    const char *channel_layouts = choose_channel_layouts(graph->output->ost);
    char args[255];
    args[0] = 0;
    if (sample_fmts) {
        av_strlcatf(args, sizeof(args), "sample_fmts=%s:", sample_fmts);
    }
    if (sample_rates) {
        av_strlcatf(args, sizeof(args), "sample_rates=%s:", sample_rates);
    }
    if (channel_layouts) {
        av_strlcatf(args, sizeof(args), "channel_layouts=%s:", channel_layouts);
    }
    AVFilter *aformat = avfilter_get_by_name("aformat");
    AVFilterContext *aformat_context;
    ret = avfilter_graph_create_filter(&aformat_context, aformat, "aformat", args, NULL, graph->graph);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    ret = avfilter_link(out->filter_ctx, 0, aformat_context, 0);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    ret = avfilter_link(aformat_context, 0, graph->output->filter, 0);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    return ret;
}

int configure_filtergraph(FilterGraph *graph) {
    int ret = 0;
    avfilter_graph_free(&graph->graph);
    if (!(graph->graph = avfilter_graph_alloc())) {
        return AVERROR(ENOMEM);
    }
    AVFilterInOut *in, *out, *cur;
    ret = avfilter_graph_parse2(graph->graph, graph->output->ost->avfilter, &in, &out);
    if (ret < 0) {
        av_err2str(ret);
        char error[255];
        av_make_error_string(error, sizeof(error), ret);
        LOGE("ffmpeg", "avfilter graph parse2 error %s", error);
        return ret;
    }
    if (!in || in->next || !out || out->next) {
        return AVERROR(EAGAIN);
    }
    int i = 0;
    for (i = 0, cur = in; cur; cur = in->next, i++) {
        switch (avfilter_pad_get_type(cur->filter_ctx->input_pads, cur->pad_idx)) {
            case AVMEDIA_TYPE_VIDEO:
                if ((ret = configure_input_video_filter(graph, cur)) < 0) {
                    avfilter_inout_free(&in);
                    avfilter_inout_free(&out);
                    return ret;
                }
                break;
            case AVMEDIA_TYPE_AUDIO:
                if ((ret = configure_input_audio_filter(graph, cur)) < 0) {
                    avfilter_inout_free(&in);
                    avfilter_inout_free(&out);
                    return ret;
                }
                break;
            default:
                break;
        }
    }
    avfilter_inout_free(&in);
    for (i = 0, cur = out; cur; cur = out->next, i++) {
        switch (avfilter_pad_get_type(cur->filter_ctx->output_pads, cur->pad_idx)) {
            case AVMEDIA_TYPE_VIDEO:
                if ((ret = configure_output_video_filter(graph, cur)) < 0) {
                    avfilter_inout_free(&out);
                    return ret;
                }
                break;
            case AVMEDIA_TYPE_AUDIO:
                if ((ret = configure_output_audio_filter(graph, cur)) < 0) {
                    avfilter_inout_free(&out);
                    return ret;
                }
                break;
            default:
                break;
        }
    }
    avfilter_inout_free(&out);
    ret = avfilter_graph_config(graph->graph, NULL);
    if (ret < 0) {
        av_err2str(ret);
        return ret;
    }
    return ret;
}

FilterGraph* init_filtergraph(InputStream *ist, OutputStream *ost) {
    FilterGraph *graph = av_mallocz(sizeof(*graph));
    if (!graph)
        return NULL;
    graph->input = av_mallocz(sizeof(*graph->input));
    graph->output = av_mallocz(sizeof(*graph->output));
    graph->input->ist = ist;
    graph->input->graph = graph;
    graph->output->ost = ost;
    graph->output->graph = graph;
    ist->filter= graph->input;
    ost->filter = graph->output;
    return graph;
}