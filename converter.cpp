#include "converter.h"

extern "C" {
   #include "libavcodec/avcodec.h"
   #include "libavformat/avformat.h"
   #include "libswscale/swscale.h"
   #include "libavdevice/avdevice.h"
}

ConverterThread::ConverterThread(QObject* parent)
    : QThread(parent) {
    curRowIndex = -1;
    curFile = nullptr;
}

void ConverterThread::run() {
    convertAllFiles();
}

int readFromSegment(void *opaque, uint8_t *buf, int buf_size) {
    QsvUnpacker* reader = (QsvUnpacker*) opaque;
    int ret = reader->read_bytes(buf, buf_size);
    return ret;
}

void ConverterThread::convertAllFiles() {
    QVector<InputFileModel>* inputFiles = dataModel->getInputFiles();
    for(curRowIndex = 0; curRowIndex < inputFiles->size(); ++curRowIndex) {
        curFile = &(*inputFiles)[curRowIndex];
        if(curFile->getStatusCode() != InputFileModel::STATUS_WAITING) {
            continue;
        }
        convertSingleFile();
    }
}

void ConverterThread::convertSingleFile() {
    curFile->setStatusCode(InputFileModel::STATUS_PROCESSING);
    curFile->setProgress(0);
    emit fileStatusChanged(curRowIndex);

    QString inputPath = curFile->getInputPath();
    QString outputPath = dataModel->getOutputPath(inputPath);

    QsvUnpacker unpacker(inputPath);
    if(unpacker.get_errcode() != 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        emit fileStatusChanged(curRowIndex);
        return;
    }
    unpacker.init_read();

    constexpr size_t BUFFER_SIZE = 0x8000;
    BYTE* iobuffer = nullptr;
    AVFormatContext* inCtx = nullptr;
    AVFormatContext* outCtx = nullptr;
    AVPacket pkt;
    float progress;

    if(!(iobuffer = (BYTE*)av_malloc(BUFFER_SIZE))) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("FFMPEG内部错误");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }
    if(!(inCtx = avformat_alloc_context())) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("FFMPEG内部错误");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }
    inCtx->pb = avio_alloc_context(iobuffer, BUFFER_SIZE, 0, &unpacker, readFromSegment, nullptr, nullptr);
    if(avformat_open_input(&inCtx, "", nullptr, nullptr) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("FFMPEG内部错误");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }
    if(avformat_find_stream_info(inCtx,0) < 0){
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("未知的流格式");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }
//    av_dump_format(inCtx, 0, inputPath.toStdString().c_str(), 0);

    avformat_alloc_output_context2(&outCtx, nullptr, nullptr, outputPath.toStdString().c_str());
    if(!outCtx) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("FFMPEG内部错误");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }
    for(unsigned int i = 0; i < inCtx->nb_streams; ++i) {
        AVStream* in_stream = inCtx->streams[i];
        AVCodec* in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
        if(!in_codec) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("未找到解码器");
            emit fileStatusChanged(curRowIndex);
            goto label_free_resources;
        }
        AVStream* out_stream = avformat_new_stream(outCtx, in_codec);
        if(!out_stream) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("FFMPEG内部错误");
            emit fileStatusChanged(curRowIndex);
            goto label_free_resources;
        }

        AVCodecContext* out_codec_ctx = avcodec_alloc_context3(in_codec);
        if(!out_codec_ctx) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("FFMPEG内部错误");
            emit fileStatusChanged(curRowIndex);
            goto label_free_resources;
        }
        avcodec_parameters_to_context(out_codec_ctx, in_stream->codecpar);
        out_codec_ctx->codec_tag = 0;
        if(outCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        if(avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx) < 0) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("FFMPEG内部错误");
            emit fileStatusChanged(curRowIndex);
            goto label_free_resources;
        }
    }
//    av_dump_format(outCtx, 0, outputPath.toStdString().c_str(), 1);

    if(avio_open(&outCtx->pb, outputPath.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("无法创建输出文件");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }

    if(avformat_write_header(outCtx, nullptr) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("不支持的输出格式");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }

    progress = unpacker.get_progress();
    curFile->setProgress(progress);
    emit fileStatusChanged(curRowIndex);

    while(av_read_frame(inCtx, &pkt) >= 0) {
        AVStream* instream = inCtx->streams[pkt.stream_index];
        AVStream* outstream = outCtx->streams[pkt.stream_index];
        pkt.pts = av_rescale_q_rnd(pkt.pts, instream->time_base, outstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, instream->time_base, outstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, instream->time_base, outstream->time_base);
        pkt.pos = -1;
        if(av_interleaved_write_frame(outCtx, &pkt) < 0) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("不支持的输出格式");
            emit fileStatusChanged(curRowIndex);
            goto label_free_resources;
        }
        av_packet_unref(&pkt);

        if(unpacker.get_progress() - progress >= 0.01) {
            progress = unpacker.get_progress();
            curFile->setProgress(progress);
            emit fileStatusChanged(curRowIndex);
        }
    }

    if(av_write_trailer(outCtx) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("不支持的输出格式");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }

    curFile->setStatusCode(InputFileModel::STATUS_SUCCEEDED);
    emit fileStatusChanged(curRowIndex);

label_free_resources:
    if(!inCtx && iobuffer) {
        av_free(iobuffer);
    }
    if(inCtx) {
        avformat_close_input(&inCtx);
    }
    if(outCtx) {
        if(outCtx->pb) {
            avio_close(outCtx->pb);
        }
        avformat_free_context(outCtx);
    }
}
