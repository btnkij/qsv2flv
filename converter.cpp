#include "converter.h"

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
    int read_size = reader->read_bytes(buf, buf_size);
    return read_size;
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

    QString inputPath = curFile->getFilePath();
    QString outputName = curFile->getFileBaseName() + dataModel->getTargetFormat();
    QString outputPath = dataModel->getOutputDir().absoluteFilePath(outputName);

    QsvUnpacker unpacker(inputPath);
    if(unpacker.get_errcode() != 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        emit fileStatusChanged(curRowIndex);
        return;
    }
    unpacker.init_progress();

    AVFormatContext* inCtx = nullptr;
    AVFormatContext* outCtx = nullptr;

    for(int i = 0; i < unpacker.get_nb_indices(); ++i) {
//        qDebug() << "segment" << i;
        unpacker.seek_to_segment(i);

        inCtx = nullptr;
        inCtx = createInputContext(&unpacker);
        if(!inCtx) {
            goto label_free_resources;
        }

        if(i == 0) {
            outCtx = nullptr;
            outCtx = createOutputContext(outputPath.toStdString().c_str(), inCtx);
            if(!outCtx) {
                goto label_free_resources;
            }

            if(avformat_write_header(outCtx, nullptr) < 0) {
                curFile->setStatusCode(InputFileModel::STATUS_FAILED);
                curFile->setStatusMsg("avformat_write_header");
                emit fileStatusChanged(curRowIndex);
                goto label_free_resources;
            }
        }

        curFile->setProgress(unpacker.get_progress());
        emit fileStatusChanged(curRowIndex);

        if(copyStreams(outCtx, inCtx, &unpacker) < 0) {
            goto label_free_resources;
        }

        avformat_close_input(&inCtx);
        inCtx = nullptr;
    }

    if(av_write_trailer(outCtx) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("av_write_trailer");
        emit fileStatusChanged(curRowIndex);
        goto label_free_resources;
    }

    curFile->setStatusCode(InputFileModel::STATUS_SUCCEEDED);
    emit fileStatusChanged(curRowIndex);

label_free_resources:
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

AVFormatContext* ConverterThread::createInputContext(QsvUnpacker* unpacker) {
    constexpr size_t BUFFER_SIZE = 0x8000;
    BYTE* buffer = nullptr;
    AVFormatContext* inCtx = nullptr;

    if(!(buffer = (BYTE*)av_malloc(BUFFER_SIZE))) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("av_malloc");
        emit fileStatusChanged(curRowIndex);
        return nullptr;
    }

    if(!(inCtx = avformat_alloc_context())) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("avformat_alloc_context");
        emit fileStatusChanged(curRowIndex);
        av_free(buffer);
        return nullptr;
    }
    inCtx->pb = avio_alloc_context(buffer, BUFFER_SIZE, 0,
                                   unpacker, readFromSegment, nullptr, nullptr);
//    inCtx->flags = AVFMT_FLAG_CUSTOM_IO;

    if(avformat_open_input(&inCtx, "", nullptr, nullptr) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("avformat_open_input");
        emit fileStatusChanged(curRowIndex);
        avformat_close_input(&inCtx);
        return nullptr;
    }

    if(avformat_find_stream_info(inCtx, 0) < 0){
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("未知的流格式");
        emit fileStatusChanged(curRowIndex);
        avformat_close_input(&inCtx);
        return nullptr;
    }

//    av_dump_format(inCtx, 0, inputPath.toStdString().c_str(), 0);

    return inCtx;
}

AVFormatContext* ConverterThread::createOutputContext(const char* outputPath, AVFormatContext* inCtx) {
    AVFormatContext* outCtx = nullptr;

    avformat_alloc_output_context2(&outCtx, nullptr, nullptr, outputPath);
    if(!outCtx) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("avformat_alloc_output_context2");
        emit fileStatusChanged(curRowIndex);
        return nullptr;
    }

    for(unsigned int i = 0; i < inCtx->nb_streams; ++i) {
        AVStream* in_stream = inCtx->streams[i];

        AVCodec* outCodec = nullptr;
        switch(inCtx->streams[i]->codecpar->codec_type) {
//        case AVMEDIA_TYPE_VIDEO:
//            qDebug() << "AV_CODEC_ID_H264";
//            outCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
//            break;
//        case AVMEDIA_TYPE_AUDIO:
//            qDebug() << "AV_CODEC_ID_AAC";
//            outCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
//            break;
        default:
            outCodec = avcodec_find_decoder(in_stream->codecpar->codec_id);
            break;
        }
        if(!outCodec) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("无法处理的流格式");
            emit fileStatusChanged(curRowIndex);
            avformat_free_context(outCtx);
            return nullptr;
        }

        AVStream* out_stream = avformat_new_stream(outCtx, outCodec);
        if(!out_stream) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("avformat_new_stream");
            emit fileStatusChanged(curRowIndex);
            avformat_free_context(outCtx);
            return nullptr;
        }

        AVCodecContext* out_codec_ctx = avcodec_alloc_context3(outCodec);
        if(!out_codec_ctx) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("avcodec_alloc_context3");
            emit fileStatusChanged(curRowIndex);
            avformat_free_context(outCtx);
            return nullptr;
        }
        avcodec_parameters_to_context(out_codec_ctx, in_stream->codecpar);
        out_codec_ctx->codec_tag = 0;
        if(outCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if(avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx) < 0) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("avcodec_parameters_from_context");
            emit fileStatusChanged(curRowIndex);
            avformat_free_context(outCtx);
            return nullptr;
        }
    }

    outCtx->oformat->flags |= AVFMT_NOTIMESTAMPS;

//    av_dump_format(outCtx, 0, outputPath.toStdString().c_str(), 1);

    if(avio_open(&outCtx->pb, outputPath, AVIO_FLAG_WRITE) < 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg("无法创建输出文件");
        emit fileStatusChanged(curRowIndex);
        avformat_free_context(outCtx);
        return nullptr;
    }

    return outCtx;
}

int ConverterThread::copyStreams(AVFormatContext* outCtx, AVFormatContext* inCtx, QsvUnpacker* unpacker) {
    AVPacket pkt;
    float progress = unpacker->get_progress();
    constexpr float PROGRESS_INTERVAL = 0.01F;

    while(av_read_frame(inCtx, &pkt) >= 0) {
        AVStream* instream = inCtx->streams[pkt.stream_index];
        AVStream* outstream = outCtx->streams[pkt.stream_index];

        pkt.pts = av_rescale_q_rnd(pkt.pts,
                                   instream->time_base,
                                   outstream->time_base,
                                   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts,
                                   instream->time_base,
                                   outstream->time_base,
                                   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, instream->time_base, outstream->time_base);
        pkt.pos = -1;

        if(av_interleaved_write_frame(outCtx, &pkt) < 0) {
            curFile->setStatusCode(InputFileModel::STATUS_FAILED);
            curFile->setStatusMsg("av_interleaved_write_frame");
            emit fileStatusChanged(curRowIndex);
            av_packet_unref(&pkt);
            return -1;
        }

        av_packet_unref(&pkt);

        if(unpacker->get_progress() - progress >= PROGRESS_INTERVAL) {
            progress = unpacker->get_progress();
            curFile->setProgress(progress);
            emit fileStatusChanged(curRowIndex);
        }
    }

    if(unpacker->get_errcode() != 0) {
        curFile->setStatusCode(InputFileModel::STATUS_FAILED);
        curFile->setStatusMsg(unpacker->get_msg());
        emit fileStatusChanged(curRowIndex);
        return -1;
    }

    return 0;
}
