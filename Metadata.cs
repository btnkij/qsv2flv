using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QSV2FLV
{
    class Metadata
    {
        public Keyframes keyframes;
        private FlvMetaItem[] MetadataArray;

        public Metadata()
        {
            keyframes.filepositions = new List<long>();
            keyframes.times = new List<double>();
            MetadataArray = new FlvMetaItem[]
            {                                                               //Index
                new FlvMetaItem("duration", AMFType.Number, null),          //0
                new FlvMetaItem("width", AMFType.Number, null),             //1
                new FlvMetaItem("height", AMFType.Number, null),
                new FlvMetaItem("videodatarate", AMFType.Number, null),
                new FlvMetaItem("framerate", AMFType.Number, null),
                new FlvMetaItem("videocodecid", AMFType.Number, null),      //5
                new FlvMetaItem("audiosamplerate", AMFType.Number, null),
                new FlvMetaItem("audiosamplesize", AMFType.Number, null),
                new FlvMetaItem("stereo", AMFType.Boolean, null),
                new FlvMetaItem("audiocodecid", AMFType.Number, null),
                new FlvMetaItem("filesize", AMFType.Number, null),          //10
                new FlvMetaItem("lasttimestamp", AMFType.Number, null),
                new FlvMetaItem("lastkeyframetimestamp", AMFType.Number, null),
                new FlvMetaItem("audiodelay", AMFType.Number, null),
                new FlvMetaItem("canSeekToEnd", AMFType.Boolean, null),
                new FlvMetaItem("creationdate", AMFType.Date, null),        //15
                new FlvMetaItem("metadatacreator", AMFType.String, null)    //16
            };
        }

        public void Dispose()
        {
            keyframes.filepositions = null;
            keyframes.times = null;
            MetadataArray = null;
        }

        public double duration
        {
            get {return (double)MetadataArray[0].Value;}
            set {MetadataArray[0].Value = value;}
        }

        public int width
        {
            get {return (int)MetadataArray[1].Value;}
            set {MetadataArray[1].Value = value;}
        }

        public int height
        {
            get {return (int)MetadataArray[2].Value;}
            set {MetadataArray[2].Value = value;}
        }

        public int videodatarate
        {
            get {return (int)MetadataArray[3].Value;}
            set {MetadataArray[3].Value = value;}
        }
        
        public double framerate
        {
            get { return (double)MetadataArray[4].Value; }
            set { MetadataArray[4].Value = value; }
        }

        public int videocodecid
        {
            get { return (int)MetadataArray[5].Value; }
            set { MetadataArray[5].Value = value; }
        }

        public int audiosamplerate
        {
            get { return (int)MetadataArray[6].Value; }
            set { MetadataArray[6].Value = value; }
        }

        public int audiosamplesize
        {
            get { return (int)MetadataArray[7].Value; }
            set { MetadataArray[7].Value = value; }
        }

        public bool stereo
        {
            get { return (bool)MetadataArray[8].Value; }
            set { MetadataArray[8].Value = value; }
        }

        public int audiocodecid
        {
            get { return (int)MetadataArray[9].Value; }
            set { MetadataArray[9].Value = value; }
        }

        public int filesize
        {
            get { return (int)MetadataArray[10].Value; }
            set { MetadataArray[10].Value = value; }
        }

        public double lasttimestamp
        {
            get { return (double)MetadataArray[11].Value; }
            set { MetadataArray[11].Value = value; }
        }

        public double lastkeyframetimestamp
        {
            get { return (double)MetadataArray[12].Value; }
            set { MetadataArray[12].Value = value; }
        }

        public int audiodelay
        {
            get { return (int)MetadataArray[13].Value; }
            set { MetadataArray[13].Value = value; }
        }

        public bool canSeekToEnd
        {
            get { return (bool)MetadataArray[14].Value; }
            set { MetadataArray[14].Value = value; }
        }

        public string creationdate
        {
            get { return (string)MetadataArray[15].Value; }
            set { MetadataArray[15].Value = value; }
        }

        public string metadatacreator
        {
            get { return (string)MetadataArray[16].Value; }
            set { MetadataArray[16].Value = value; }
        }

        public struct Keyframes
        {
            public List<long> filepositions;
            public List<double> times;

            public void Add(long pos, double t)
            {
                filepositions.Add(pos);
                times.Add(t);
            }

            public int Count
            {
                get { return filepositions.Count; }
            }
        }

        public struct FlvMetaItem
        {
            public string Key;
            public AMFType Type;
            public object Value;

            public FlvMetaItem(string _key, AMFType _type, object _value)
            {
                Key = _key;
                Type = _type;
                Value = _value;
            }
        }

        public enum AMFType
        {
            Number = 0,
            Boolean = 1,
            String = 2,
            Object = 3,
            MovieClip = 4,
            Null = 5,
            Undefined = 6,
            Reference = 7,
            ECMAArray = 8,
            StrictArray = 10,
            Date = 11,
            LongString = 12
        }

        public const int metadatasize_position = 0xE;
        public const int duration_position = 0x35;
        public const int width_position = 0x45;
        public const int height_position = 0x56;
        public const int videodatarate_position = 0x6E;
        public const int framerate_position = 0x82;
        public const int videocodecid_position = 0x99;
        public const int audiosamplerate_position = 0xB3;
        public const int audiosamplesize_position = 0xCD;
        public const int stereo_position = 0xDE;
        public const int audiocodecid_position = 0xEE;
        public const int filesize_position = 0x101;
        public const int lasttimestamp_position = 0x119;
        public const int lastkeyframetimestamp_position = 0x139;
        public const int audiodelay_position = 0x14E;
        public const int canSeekToEnd_position = 0x165;
        public const int creationdate_position = 0x177;
        public const int metadatacreator_position = 0x19A;

        public static byte[] Meta_1
        {
            get
            {
                return new byte[] 
                { 
                    0x46, 0x4C, 0x56, 0x01, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0A, 0x6F, 0x6E, 0x4D, 0x65, 0x74, 
                    0x61, 0x44, 0x61, 0x74, 0x61, 0x08, 0x00, 0x00, 0x00, 0x12, 0x00, 0x08, 0x64, 0x75, 0x72, 0x61, 
                    0x74, 0x69, 0x6F, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x77, 
                    0x69, 0x64, 0x74, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x68, 
                    0x65, 0x69, 0x67, 0x68, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 
                    0x76, 0x69, 0x64, 0x65, 0x6F, 0x64, 0x61, 0x74, 0x61, 0x72, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x66, 0x72, 0x61, 0x6D, 0x65, 0x72, 0x61, 0x74, 
                    0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x76, 0x69, 0x64, 0x65, 
                    0x6F, 0x63, 0x6F, 0x64, 0x65, 0x63, 0x69, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x0F, 0x61, 0x75, 0x64, 0x69, 0x6F, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x72, 0x61, 
                    0x74, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x61, 0x75, 0x64, 
                    0x69, 0x6F, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x73, 0x69, 0x7A, 0x65, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x73, 0x74, 0x65, 0x72, 0x65, 0x6F, 0x01, 0x00, 0x00, 
                    0x0C, 0x61, 0x75, 0x64, 0x69, 0x6F, 0x63, 0x6F, 0x64, 0x65, 0x63, 0x69, 0x64, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x69, 0x7A, 0x65, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x6C, 0x61, 0x73, 0x74, 0x74, 
                    0x69, 0x6D, 0x65, 0x73, 0x74, 0x61, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x15, 0x6C, 0x61, 0x73, 0x74, 0x6B, 0x65, 0x79, 0x66, 0x72, 0x61, 0x6D, 0x65, 0x74, 
                    0x69, 0x6D, 0x65, 0x73, 0x74, 0x61, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x0A, 0x61, 0x75, 0x64, 0x69, 0x6F, 0x64, 0x65, 0x6C, 0x61, 0x79, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x63, 0x61, 0x6E, 0x53, 0x65, 0x65, 0x6B, 0x54, 
                    0x6F, 0x45, 0x6E, 0x64, 0x01, 0x00, 0x00, 0x0C, 0x63, 0x72, 0x65, 0x61, 0x74, 0x69, 0x6F, 0x6E, 
                    0x64, 0x61, 0x74, 0x65, 0x02, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x6D, 0x65, 0x74, 0x61, 0x64, 0x61, 
                    0x74, 0x61, 0x63, 0x72, 0x65, 0x61, 0x74, 0x6F, 0x72, 0x02 
                };
            }
        }

        public static byte[] Meta_2
        {
            get
            {
                return new byte[] 
                { 
                    0x00, 0x09, 0x6B, 0x65, 0x79, 0x66, 0x72, 0x61, 0x6D, 0x65, 0x73, 0x03, 0x00, 0x0D, 0x66, 0x69,
                    0x6C, 0x65, 0x70, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x73, 0x0A 
                };
            }
        }

        public static byte[] Meta_3
        {
            get
            {
                return new byte[] { 0x00, 0x05, 0x74, 0x69, 0x6D, 0x65, 0x73, 0x0A };
            }
        }

        public static byte[] Meta_4
        {
            get
            {
                return new byte[] { 0x00, 0x00, 0x09, 0x00, 0x00, 0x09 };
            }
        }
    }
}
