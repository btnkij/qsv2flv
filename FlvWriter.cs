using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Windows.Forms;
using System.Diagnostics;

namespace QSV2FLV
{
    public class FlvWriter
    {
        private FileStream flv; //Completed FLV File
        private FileStream source; //FLV without header and metadata
        private Metadata meta;//Metadata
        private string path;

        /// <summary>
        /// General
        /// </summary>
        public string FullName
        {
            get { return path; }
        }

        public FileStream BaseStream
        {
            get { return flv; }
        }

        //Constructed Code
        public FlvWriter(string outputPath, string sourcePath)
        {
            path = outputPath;
            FileInfo file = new FileInfo(path);
            if (!Directory.Exists(file.DirectoryName))
            {
                Directory.CreateDirectory(file.DirectoryName);
            }
            if (file.Exists)
            {
                file.Delete();
            }
            flv = new FileStream(path, FileMode.CreateNew);
            source = new FileStream(sourcePath, FileMode.Open);
        }

        private void WriteTag(byte[] tag)
        {
            flv.Seek(0L, SeekOrigin.End);
            flv.Write(tag, 0, tag.GetLength(0));
        }

        public void Dispose()
        {
            source.Close();
            source.Dispose();
            flv.Close();
            flv.Dispose();
            meta.Dispose();
            meta = null;
        }

        /// <summary>
        /// Flv Metadata Generator
        /// </summary>
        public void Parse()
        {
            meta = new Metadata();
            SeekLastTag();
            meta.duration = GetTimeStamp() / 1000D;
            meta.lasttimestamp = GetTimeStamp() / 1000D;
            SeekFirstTag();
            while (true)
            {
                if (GetTagType() != 9)
                {
                    SeekNextTag();
                    continue;
                }
                if (IsKeyFrame())
                {
                    int[] info = ParseVideoTag();
                    meta.videocodecid = info[0];
                    break;
                }
            }
            while (true)
            {
                if (GetTagType() != 8)
                {
                    SeekNextTag();
                    continue;
                }
                int[] info = ParseAudioTag();
                meta.audiocodecid = info[0];
                meta.audiosamplerate = info[1];
                meta.audiosamplesize = info[2];
                meta.stereo = info[3] == 1 ? true : false;
                break;
            }
            int i = 0;
            int[] samples = new int[6];
            while (i <= samples.GetUpperBound(0))
            {
                if (GetTagType() != 9 || GetTimeStamp() < 1)
                {
                    SeekNextTag();
                    continue;
                }
                samples[i] = GetTimeStamp();
                SeekNextTag();
                i++;
            }
            meta.framerate = 9000D / (samples[5] + samples[4] + samples[3] - samples[2] - samples[1] - samples[0]); //Framerate UNIT = fps
            const int breaking = 2000; //Breaking between two keyframes. UNIT = ms
            int lastTimestamp = -breaking;
            SeekFirstTag();
            while (IsNotEnd())
            {
                if (GetTagType() != 9)
                {
                    SeekNextTag();
                    continue;
                }
                if (!IsKeyFrame())
                {
                    SeekNextTag();
                    continue;
                }
                int timestamp = GetTimeStamp();
                if (timestamp - lastTimestamp >= breaking)
                {
                    meta.keyframes.Add(Position, GetTimeStamp() / 1000D);
                    lastTimestamp = timestamp;
                }
                SeekNextTag();
            }
            Position = meta.keyframes.filepositions.Last();
            while (IsNotEnd())
            {
                if (GetTagType() != 9)
                {
                    SeekNextTag();
                    continue;
                }
                if (IsKeyFrame())
                {
                    meta.lastkeyframetimestamp = GetTimeStamp() / 1000D;
                }
                SeekNextTag();
            }
            meta.width = 0;
            meta.height = 0;
            meta.videodatarate = 0;
            meta.filesize = 0;
            meta.audiodelay = 0;
            meta.canSeekToEnd = true;
            meta.creationdate = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
            meta.metadatacreator = "QSV2FLV v2.0 by Var";
        }

        public void Output()
        {
            //FLV Base Info
            flv.Seek(0, SeekOrigin.Begin);
            flv.Write(Metadata.Meta_1, 0, Metadata.Meta_1.GetLength(0));
            flv.Seek(Metadata.duration_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.duration), 0, 8);
            //flv.Seek(Metadata.width_position, SeekOrigin.Begin);
            //flv.Write(DoubleToBytes(meta.width), 0, 8);
            //flv.Seek(Metadata.height_position, SeekOrigin.Begin);
            //flv.Write(DoubleToBytes(meta.height), 0, 8);
            flv.Seek(Metadata.videodatarate_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.videodatarate), 0, 8);
            flv.Seek(Metadata.framerate_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.framerate), 0, 8);
            flv.Seek(Metadata.videocodecid_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.videocodecid), 0, 8);
            flv.Seek(Metadata.audiosamplerate_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.audiosamplerate), 0, 8);
            flv.Seek(Metadata.audiosamplesize_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.audiosamplesize), 0, 8);
            flv.Seek(Metadata.stereo_position, SeekOrigin.Begin);
            flv.Write(BoolToBytes(meta.stereo), 0, 1);
            flv.Seek(Metadata.audiocodecid_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.audiocodecid), 0, 8);
            //flv.Seek(Metadata.filesize_position, SeekOrigin.Begin);
            //flv.Write(DoubleToBytes(meta.filesize), 0, 8);
            flv.Seek(Metadata.lasttimestamp_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.lasttimestamp), 0, 8);
            flv.Seek(Metadata.lastkeyframetimestamp_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(meta.lastkeyframetimestamp), 0, 8);
            //flv.Seek(Metadata.audiodelay_position, SeekOrigin.Begin);
            //flv.Write(DoubleToBytes(meta.audiodelay), 0, 8);
            flv.Seek(Metadata.canSeekToEnd_position, SeekOrigin.Begin);
            flv.Write(BoolToBytes(meta.canSeekToEnd), 0, 1);
            flv.Seek(Metadata.creationdate_position, SeekOrigin.Begin);
            flv.Write(StringToBytes(meta.creationdate.ToString()), 0, 11);
            flv.Seek(Metadata.metadatacreator_position, SeekOrigin.Begin);
            flv.Write(ShortToBytes(meta.metadatacreator.Length), 0, 2);
            flv.Write(StringToBytes(meta.metadatacreator), 0, meta.metadatacreator.Length);

            //Keyframes
            flv.Seek(0, SeekOrigin.End);
            flv.Write(Metadata.Meta_2, 0, Metadata.Meta_2.GetLength(0));
            int headerSize = meta.keyframes.Count * 18 + meta.metadatacreator.Length + 466;
            //filepositions
            flv.Write(IntToBytes(meta.keyframes.Count), 0, 4);
            foreach (var i in meta.keyframes.filepositions)
            {
                flv.WriteByte(0);
                flv.Write(DoubleToBytes(i + headerSize), 0, 8);
            }
            //times
            flv.Write(Metadata.Meta_3, 0, Metadata.Meta_3.GetLength(0));
            flv.Write(IntToBytes(meta.keyframes.Count), 0, 4);
            foreach (var i in meta.keyframes.times)
            {
                flv.WriteByte(0);
                flv.Write(DoubleToBytes(i), 0, 8);
            }

            //Pre-tag Size
            flv.Seek(Metadata.metadatasize_position, SeekOrigin.Begin);
            flv.Write(IntToBytes((int)flv.Length - 18), 1, 3);
            flv.Seek(0, SeekOrigin.End);
            flv.Write(Metadata.Meta_4, 0, Metadata.Meta_4.GetLength(0));
            flv.Write(IntToBytes((int)flv.Length - 13), 0, 4);

            //Copy Source Stream
            source.Seek(0, SeekOrigin.Begin);
            flv.Seek(0, SeekOrigin.End);
            const int bufferSize = 10485760;//10MB
            byte[] buffer = new byte[bufferSize];
            while (source.Read(buffer, 0, bufferSize) == bufferSize)
                flv.Write(buffer, 0, bufferSize);
            int size = (int)(source.Length % (long)bufferSize);
            source.Read(buffer, 0, bufferSize);
            flv.Write(buffer, 0, size);

            //Metadata.filesize
            flv.Seek(Metadata.filesize_position, SeekOrigin.Begin);
            flv.Write(DoubleToBytes(flv.Length), 0, 4);
        }

        /// <summary>
        /// Flv Parse Tools
        /// </summary>
        private long Position
        {
            get { return source.Position; }
            set { source.Position = value; }
        }

        private void SeekFirstTag()
        {
            source.Seek(0, SeekOrigin.Begin);
        }

        private void SeekNextTag()
        {
            byte[] buffer = new byte[4];
            source.Read(buffer, 0, 4);
            if (!(buffer[0] == 8 || buffer[0] == 9))
                throw new Exception("Unknown tag.");
            int datasize = (0xFF & buffer[1]) << 16 | (0xFF & buffer[2]) << 8 | (0xFF & buffer[3]);
            try
            {
                source.Seek(datasize + 11, SeekOrigin.Current);
            }
            catch
            {
                throw;
            }
        }

        private void SeekLastTag()
        {
            byte[] buffer = new byte[4];
            source.Seek(-4, SeekOrigin.End);
            source.Read(buffer, 0, 4);
            int tagSize =
                (0xFF & buffer[0]) << 24 | (0xFF & buffer[1]) << 16 | (0xFF & buffer[2]) << 8 | (0xFF & buffer[3]);
            source.Seek(-tagSize - 4, SeekOrigin.End);
        }

        private bool IsNotEnd()
        {
            return source.Position < source.Length;
        }

        private int[] ParseAudioTag()
        {
            if (source.ReadByte() != 8)
                throw new Exception("Not a audio tag.");
            source.Seek(10, SeekOrigin.Current);
            int buffer = source.ReadByte();
            source.Seek(-12, SeekOrigin.Current);
            int SoundFormat = buffer >> 4;
            int SoundRate = buffer >> 2 & 3;
            int SoundSize = buffer >> 1 & 1;
            int SoundType = buffer & 1;
            return new int[]
            {
                SoundFormat,    //audiocodecid
                SoundRate,      //audiosamplerate
                SoundSize,      //audiosamplesize
                SoundType       //stereo
            };
        }

        private int[] ParseVideoTag()
        {
            if (source.ReadByte() != 9)
                throw new Exception("Not a video tag.");
            source.Seek(10, SeekOrigin.Current);
            int buffer = source.ReadByte();
            source.Seek(-12, SeekOrigin.Current);
            if ((buffer & 17) != 17)
                throw new Exception("Not a keyframe.");
            int CodecID = buffer & 15;
            return new int[] 
            { 
                CodecID         //videocodecid
            };
        }

        private bool IsKeyFrame()
        {
            if (source.ReadByte() != 9)
                throw new Exception("Not a video tag.");
            source.Seek(10, SeekOrigin.Current);
            int buffer = source.ReadByte();
            source.Seek(-12, SeekOrigin.Current);
            return (buffer & 17) == 17 ? true : false;
        }

        private int GetTimeStamp()
        {
            byte[] buffer = new byte[4];
            source.Seek(4, SeekOrigin.Current);
            source.Read(buffer, 0, 4);
            source.Seek(-8, SeekOrigin.Current);
            int timestamp =
                (0xFF & buffer[0]) << 16 | (0xFF & buffer[1]) << 8 | (0xFF & buffer[2]) | (0xFF & buffer[3]) << 24;
            return timestamp;
        }

        private int GetTagType()
        {
            int type = source.ReadByte();
            source.Seek(-1, SeekOrigin.Current);
            return type;
        }

        private byte[] IntToBytes(int num)
        {
            byte[] buffer = BitConverter.GetBytes((int)num);
            Array.Reverse(buffer);
            return buffer;
        }
        private byte[] ShortToBytes(int num)
        {
            byte[] buffer = BitConverter.GetBytes(num);
            return new byte[] { buffer[1], buffer[0] };
        }
        private byte[] DoubleToBytes(double num)
        {
            byte[] buffer = BitConverter.GetBytes((double)num);
            Array.Reverse(buffer);
            return buffer;
        }
        private byte[] StringToBytes(string str)
        {
            return System.Text.Encoding.Default.GetBytes(str);
        }
        private byte[] BoolToBytes(bool bl)
        {
            return BitConverter.GetBytes(bl);
        }
    }
}
