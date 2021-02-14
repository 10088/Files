﻿using System;

namespace Files.Common
{
    public class ShellFileItem
    {
        public bool IsFolder;
        public string RecyclePath;
        public string FileName;
        public string FilePath;
        public long RecycleDate;
        public long ModifiedDate;
        public long CreatedDate;
        public string FileSize;
        public ulong FileSizeBytes;
        public string FileType;

        public DateTime RecycleDateDT => DateTime.FromFileTimeUtc(RecycleDate).ToLocalTime();
        public DateTime ModifiedDateDT => DateTime.FromFileTimeUtc(ModifiedDate).ToLocalTime();
        public DateTime CreatedDateDT => DateTime.FromFileTimeUtc(CreatedDate).ToLocalTime();

        public ShellFileItem()
        {
        }

        public ShellFileItem(
            bool isFolder, string recyclePath, string fileName, string filePath,
            long recycleDate, long modifiedDate, long createdDate, string fileSize, ulong fileSizeBytes, string fileType)
        {
            this.IsFolder = isFolder;
            this.RecyclePath = recyclePath;
            this.FileName = fileName;
            this.FilePath = filePath;
            this.RecycleDate = recycleDate;
            this.ModifiedDate = modifiedDate;
            this.CreatedDate = createdDate;
            this.FileSize = fileSize;
            this.FileSizeBytes = fileSizeBytes;
            this.FileType = fileType;
        }
    }
}