# Phlex_IO_prototype

#Prototype development for I/O infrastructure supporting Phlex

#setup ROOT (sorry that's ATLAS, but pick your poison;)
lsetup "root 6.34.04-x86_64-el9-gcc13-opt"
export CMAKE_PREFIX_PATH=/cvmfs/sft.cern.ch/lcg/releases/LCG_107a_ATLAS_2/vdt/0.4.4/aarch64-el9-gcc13-opt:${CMAKE_PREFIX_PATH} # vdt hack

#build
cmake ../Phlex_IO_prototype/ ; make

#run writer
./phlex_Driver/Phlex_Writer ; ls -l toy.root

#ROOT checks:
>>> TFile* file =  TFile::Open("toy.root")
>>> file->ls()
>>> TTree* tree1 = (TTree*)file->Get("<ToyAlg_Segment>");
>>> tree1->Print()
>>> TTree* tree2 = (TTree*)file->Get("<ToyAlg_Event>");
>>> tree2->Print()
>>> tree1->Scan()
>>> tree2->Scan()

#run reader
./phlex_Driver/Phlex_Reader
