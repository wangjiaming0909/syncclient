#!/bin/bash

echo 'generating proto files'
protoc -I=. --cpp_out=. ./sync_mess.proto \
&& mv ./sync_mess.pb.cc ./sync_mess.pb.cpp
