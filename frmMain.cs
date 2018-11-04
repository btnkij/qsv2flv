using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace QSV2FLV
{
    public partial class frmMain : Form
    {
        private string output = "";
        public static bool reqPause = false;
        private Thread task;

        public frmMain()
        {
            InitializeComponent();
            //backgroundWorker1.DoWork += backgroundWorker1_DoWork;
            //backgroundWorker1.RunWorkerCompleted += backgroundWorker1_RunWorkerCompleted;
            listView.Items.Clear();
            tbxOutput.Text = @"D:\Output";
            output = tbxOutput.Text;
        }

        /*void backgroundWorker1_DoWork(object sender, DoWorkEventArgs e)
        {
            ListViewItem item = listView.Items[index];
            //MessageBox.Show("1");
            item.SubItems[1].Text = "Transcoding";
            //MessageBox.Show("2");
            QsvTranscoder qsvTrans = new QsvTranscoder(item.SubItems[0].Text, tbxOutput.Text);
            //MessageBox.Show("3");
            //qsvTrans.Transcode();
            Thread trans = new Thread(new ThreadStart(qsvTrans.Transcode));
            //MessageBox.Show("4");
            trans.IsBackground = true;
            trans.Start();
            //MessageBox.Show("5");
            //while (trans.IsAlive) ;
            //item.SubItems[1].Text = "Completed";
            //throw new NotImplementedException();
        }

        void backgroundWorker1_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            listView.Items[index].SubItems[1].Text = "Completed";
            index++;
            if (index < listView.Items.Count)
            {
                backgroundWorker1.RunWorkerAsync();
            }
        }*/

        private void btnTrans_Click(object sender, EventArgs e)
        {
            if (tbxOutput.Text == "")
            {
                MessageBox.Show("Invalid output path.", "Warning",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button1);
                return;
            }
            if (listView.Items.Count < 1)
            {
                return;
            }

            btnTrans.Enabled = false;
            btnImport.Enabled = false;
            btnRemove.Enabled = false;
            btnClear.Enabled = false;
            btnPause.Enabled = true;
            btnContinue.Enabled = false;
            tbxOutput.Enabled = false;
            btnOutput.Enabled = false;

            output = tbxOutput.Text;
            task = new Thread(new ThreadStart(TaskGroup));
            task.IsBackground = true;
            task.Start();
            //for (int i = 0; i < listView.Items.Count; i++)
            //{
            //    ThreadPool.QueueUserWorkItem(new WaitCallback(Transcode),i);
            //}
            //backgroundWorker1.RunWorkerAsync();
            //ThreadPool.SetMaxThreads(1000, 1);
            //foreach (ListViewItem item in listView.Items)
            //{
                //ThreadPool.QueueUserWorkItem(new WaitCallback(Transcode), new TaskInfo(item, tbxOutput.Text));
                //item = _item;       
                //backgroundWorker1.RunWorkerAsync();
                //while (backgroundWorker1.IsBusy) ;
                //QsvTranscoder qsvTrans = new QsvTranscoder(item, tbxOutput.Text);
                //Thread trans = new Thread(new ThreadStart(qsvTrans.Transcode));
                //ThreadPool.QueueUserWorkItem(new WaitCallback(qsvTrans.Transcode));
            //}       
        }

        private void btnImport_Click(object sender, EventArgs e)
        {
            openFileDialog1.ShowDialog();
            listView.Items.Clear();
            foreach (string file in openFileDialog1.FileNames)
            {
                ListViewItem item = new ListViewItem(file);
                item.SubItems.Add("Waiting");
                listView.Items.Add(item);
            }
        }

        private void btnRemove_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem item in listView.SelectedItems)
            {
                item.Remove();
            }
        }

        private void btnClear_Click(object sender, EventArgs e)
        {
            listView.Items.Clear();
        }

        private void btnPause_Click(object sender, EventArgs e)
        {
            reqPause = true;

            btnTrans.Enabled = false;
            btnImport.Enabled = true;
            btnRemove.Enabled = true;
            btnClear.Enabled = true;
            btnPause.Enabled = false;
            btnContinue.Enabled = true;
            tbxOutput.Enabled = true;
            btnOutput.Enabled = true;
        }

        private void btnContinue_Click(object sender, EventArgs e)
        {
            reqPause = false;

            btnTrans.Enabled = false;
            btnImport.Enabled = false;
            btnRemove.Enabled = false;
            btnClear.Enabled = false;
            btnPause.Enabled = true;
            btnContinue.Enabled = false;
            tbxOutput.Enabled = false;
            btnOutput.Enabled = false;
        }

        private void btnAbout_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Programmer : Var\nVersion : 2.0", "About");
        }

        private void btnOutput_Click(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            tbxOutput.Text = folderBrowserDialog1.SelectedPath;
        }

        private void Transcode(object state)
        {
            int i = (int)state;
            string qsvPath = "";
            this.Invoke(new EventHandler(delegate
            {
                listView.Items[i].SubItems[1].Text = "Transcoding";
                qsvPath = listView.Items[i].SubItems[0].Text;
            }));
            Transcoder trans = new Transcoder(qsvPath, output);
            trans.Transcode();
            trans.Dispose();
            this.Invoke(new EventHandler(delegate
            {
                listView.Items[i].SubItems[1].Text = "Completed";
            }));
        }

        private void TaskGroup()
        {
            int count = -1;
            this.Invoke(new EventHandler(delegate
            {
                count = listView.Items.Count;
            }));
            for (int i = 0; i < count; i++)
            {
                Transcode(i);
            }
            this.Invoke(new EventHandler(delegate
            {
                btnTrans.Enabled = true;
                btnImport.Enabled = true;
                btnRemove.Enabled = true;
                btnClear.Enabled = true;
                btnPause.Enabled = false;
                btnContinue.Enabled = false;
                tbxOutput.Enabled = true;
                btnOutput.Enabled = true;
            }));
           
        }
    }
}
