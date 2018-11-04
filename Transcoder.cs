using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Windows.Forms;

namespace QSV2FLV
{
    public class Transcoder
    {
        private FileStream temp, qsv;
        private FlvWriter flv;
        private string qsvPath, outputPath, outputName;

        /// <summary>
        /// Transcode
        /// </summary>
        //Constructed Code
        public Transcoder(string _qsvPath, string _outputPath) 
        {
            qsvPath = _qsvPath;
            outputPath = _outputPath + "\\";
            qsv = new FileStream(qsvPath, FileMode.Open);
            FileInfo file = new FileInfo(qsvPath);
            outputName = file.Name.Substring(0, file.Name.LastIndexOf("."));
            file = new FileInfo(outputPath + outputName + ".temp");
            if (!Directory.Exists(file.DirectoryName))
                Directory.CreateDirectory(file.DirectoryName);
            if (file.Exists)
                file.Delete();
            temp = new FileStream(outputPath + outputName + ".temp",FileMode.CreateNew);
        }

        public void Dispose()
        {
            flv.Dispose();
            FileInfo file = new FileInfo(outputPath + outputName + ".temp");
            file.Delete();
        }

        public void Transcode()
        {
            SeekBegin();
            SkipMeta();
            while (true)
            {
                try
                {
                    if (ReadNext())
                    {
                        byte[] buffer = GetTag();
                        temp.Write(buffer, 0, buffer.GetLength(0));
                    }
                    else
                    {
                        SkipMeta();
                        SeekNextTag();
                        SeekNextTag();
                    }
                }
                catch (Exception e)
                {
                    if (e is EndOfStreamException) break;
                    else throw;
                }
            }
            qsv.Close();
            qsv.Dispose();
            temp.Close();
            temp.Dispose();
            flv = new FlvWriter(outputPath + outputName + ".flv", outputPath + outputName + ".temp");
            flv.Parse();
            flv.Output();
        }

        /// <summary>
        /// Transcode Tools
        /// </summary>
        private bool CheckTAG()
        {
            byte[] buffer = new byte[10];
            byte[] tag = Encoding.ASCII.GetBytes("QIYI VIDEO");
            qsv.Seek(0L, SeekOrigin.Begin);
            qsv.Read(buffer, 0, 10);
            return buffer == tag ? true : false;
        }

        private bool CheckVersion()
        {
            byte[] buffer = new byte[4];
            qsv.Seek(10L, SeekOrigin.Begin);
            qsv.Read(buffer, 0, 4);
            return BytesToInt(buffer, 0) == 2 ? true : false;
        }

        private void SeekBegin()
        {
            long offset = 0L;
            int size = 0;
            byte[] buffer = new byte[12];
            qsv.Seek(74L, SeekOrigin.Begin);
            qsv.Read(buffer, 0, 12);
            offset = BytesToLong(buffer, 0);
            size = BytesToInt(buffer, 8);
            qsv.Seek(offset + size, SeekOrigin.Begin);
        }

        private void SkipMeta()
        {
            qsv.Position += 0xD;
            int len = 0;
            while (true)
            {
                int bit = qsv.ReadByte();
                ++len;
                if (bit == 0x9)
                {
                    byte[] buffer = new byte[4];
                    qsv.Read(buffer, 0, 4);
                    int size =
                        (0xFF & buffer[0]) << 24 | (0xFF & buffer[1]) << 16 | (0xFF & buffer[2]) << 8 | (0xFF & buffer[3]);
                    if (len == size) break;
                    else qsv.Position -= 4;
                }
            }
        }

        private void SeekNextTag()
        {
            int dataSize = 0;
            byte[] buffer = new byte[4];
            qsv.Read(buffer, 0, 4);
            dataSize = (0xFF & buffer[1]) << 16 | (0xFF & buffer[2]) << 8 | (0xFF & buffer[3]);
            qsv.Seek(dataSize + 11, SeekOrigin.Current);
        }

        private byte[] GetTag()
        {
            int dataSize = 0;
            byte[] buffer = new byte[4];
            qsv.Read(buffer, 0, 4);
            qsv.Position -= 4;
            dataSize = (0xFF & buffer[1]) << 16 | (0xFF & buffer[2]) << 8 | (0xFF & buffer[3]);
            byte[] tag = new byte[dataSize + 15];
            qsv.Read(tag, 0, dataSize + 15);
            return tag;
        }

        private bool ReadNext()
        {
            if (qsv.Position >= qsv.Length - 11)
            {
                throw new EndOfStreamException();
            }
            byte[] buffer = new byte[11];
            qsv.Read(buffer, 0, 11);
            qsv.Position -= 11;
            if ((buffer[8] | buffer[9] | buffer[10]) == 0 && (buffer[0] == 0x8 || buffer[0] == 0x9))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        private int BytesToInt(byte[] paramArrayOfByte, int paramInt)
        {
            return 0xFF & paramArrayOfByte[paramInt] | (0xFF & paramArrayOfByte[(paramInt + 1)]) << 8 | (0xFF & paramArrayOfByte[(paramInt + 2)]) << 16 | (0xFF & paramArrayOfByte[(paramInt + 3)]) << 24;
        }

        private long BytesToLong(byte[] paramArrayOfByte, int paramInt)
        {
            int i = 0xFF & paramArrayOfByte[paramInt] | (0xFF & paramArrayOfByte[(paramInt + 1)]) << 8 | (0xFF & paramArrayOfByte[(paramInt + 2)]) << 16 | (0xFF & paramArrayOfByte[(paramInt + 3)]) << 24;
            int j = 0xFF & paramArrayOfByte[(paramInt + 4)] | (0xFF & paramArrayOfByte[(paramInt + 5)]) << 8 | (0xFF & paramArrayOfByte[(paramInt + 6)]) << 16 | (0xFF & paramArrayOfByte[(paramInt + 7)]) << 24;
            return i + j;
        }
    }
}
