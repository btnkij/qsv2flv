#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qsvreader.h"
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    QsvUnpacker* reader = (QsvUnpacker*) opaque;
    int ret = reader->read_bytes(buf, buf_size);
//    qDebug("ret %d", ret);
    return ret;
}

void MainWindow::on_pushButton_clicked()
{
    QString input_path
            = "D:\\Application\\cpp\\build-qsv2flv-Desktop_Qt_6_1_1_MSVC2019_64bit-Debug\\debug\\testfile\\3.qsv";
    QString output_path
            = "D:\\Application\\cpp\\build-qsv2flv-Desktop_Qt_6_1_1_MSVC2019_64bit-Debug\\debug\\testfile\\3.mp4";

    QsvUnpacker unpacker(input_path);
    if(unpacker.get_errcode() != 0) {
        qDebug() << unpacker.get_msg();
    }
    AVFormatContext* ic = avformat_alloc_context();
    BYTE* iobuffer = (BYTE*)av_malloc(0x8000);
    unpacker.init_read();
    ic->pb = avio_alloc_context(iobuffer, 0x8000, 0, &unpacker, read_packet, NULL, NULL);
    int err = avformat_open_input(&ic, "", nullptr, nullptr);
    qDebug("err %d", err);
    if(avformat_find_stream_info(ic,0) < 0){
        qDebug() <<  "Failed to retrieve input stream information";
    }
    av_dump_format(ic, 0, input_path.toStdString().c_str(), 0);

    AVFormatContext* oc = nullptr;
    avformat_alloc_output_context2(&oc, nullptr, nullptr, output_path.toStdString().c_str());

    for(unsigned int i = 0; i < ic->nb_streams; ++i) {
        AVStream* in_stream = ic->streams[i];
        AVCodec* in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
        if(!in_codec) {
            qDebug() << "avcodec_find_decoder error";
        }
        AVStream* out_stream = avformat_new_stream(oc, in_codec);
        if(!out_stream) {
            qDebug() << "avformat_new_stream error";
        }

        AVCodecContext* out_codec_ctx = avcodec_alloc_context3(in_codec);
        if(!out_codec_ctx) {
            qDebug() << "avcodec_alloc_context3 error";
        }
        avcodec_parameters_to_context(out_codec_ctx, in_stream->codecpar);
        out_codec_ctx->codec_tag = 0;
        if(oc->oformat->flags & AVFMT_GLOBALHEADER) {
            out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        if(avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx) < 0) {
            qDebug() << "avcodec_parameters_from_context error";
        }
    }
    av_dump_format(oc, 0, output_path.toStdString().c_str(), 1);

    if(avio_open(&oc->pb, output_path.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
        qDebug() << "avio_open error";
    }

    if(avformat_write_header(oc, nullptr) < 0) {
        qDebug() << "avformat_write_header error";
    }

    AVPacket pkt;
    while(av_read_frame(ic, &pkt) >= 0) {
        AVStream* instream = ic->streams[pkt.stream_index];
        AVStream* outstream = oc->streams[pkt.stream_index];
        pkt.pts = av_rescale_q_rnd(pkt.pts, instream->time_base, outstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, instream->time_base, outstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, instream->time_base, outstream->time_base);
        pkt.pos = -1;
        if(av_interleaved_write_frame(oc, &pkt) < 0) {
            qDebug() << "Error muxing packet";
            break;
        }
        av_packet_unref(&pkt);
    }

    av_write_trailer(oc);

    avformat_close_input(&ic);
    avio_close(oc->pb);
    avformat_free_context(oc);
    av_free(iobuffer);
}

