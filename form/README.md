# FORM (Fine-grained Object Reading/Writing Model)

Prototype development for I/O infrastructure supporting Phlex

## build

`cmake -DUSE_FORM_ALONE=ON -DFORM_USE_ROOT_STORAGE=ON ../phlex/ ; make`

## run writer

`./test/form/phlex_writer ; ls -l toy.root`

## ROOT checks:
```
TFile* file =  TFile::Open("toy.root")
file->ls()
TTree* tree1 = (TTree*)file->Get("<ToyAlg_Segment>");
tree1->Print()
TTree* tree2 = (TTree*)file->Get("<ToyAlg_Event>");
tree2->Print()
tree1->Scan()
tree2->Scan()
```

# run reader

`./test/form/phlex_reader`
