# Project 5: Resource Management
The goal is to learn about resource management inside an operating system. This project will also include deadlock detection and management. 

I encountered many bugs and errors along the way with the previous project, so I wanted to start a little fresh and clean up as I went. This was the last project I had nearly finished last semester before I left, so I will starting fresh from that. 

# How to Run: 
1. run 'make' 
2. ./oss [-h] [-n proc] [-s simul] [-i intervalInMs] [-f logfile] 
3.  


## Deadlock Algorithm: 
The deadlock policy will be an attempt at the system managing resources among processes to resolve detected deadlocks. Child processes request or release resources and may terminate after 250ms. The parent will check for deadlocks when granting resources. If the deadlock is detected - the system will initially attempt to satisfy any ongoing resource requests. If the persistence continues, the system will attempt to resolve it by removing the child that has done the least amount of work. 
