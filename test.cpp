TH2D *his;

void test()
{
   his = new TH2D("his", "test", 1024, 0., 1024., 30000, 0., 30000.);
   
   auto fileName = "ch2and3.root";
   auto file = new TFile(fileName, "READ");
   auto tree = (TTree*)file->Get("fine");

   UChar_t ch;
   tree->SetBranchAddress("ch", &ch);

   UShort_t adc;
   tree->SetBranchAddress("adc", &adc);

   ULong64_t fineTS;
   tree->SetBranchAddress("fine", &fineTS);

   const auto nEve = tree->GetEntries();
   for(auto i = 0; i < nEve; i++){
      tree->GetEntry(i);

      //cout << Int_t(ch) <<"\t"<< fineTS << endl;
      if(ch == 3
         //
         && (adc > 4750 && adc < 5100)
         //&& (adc > 3300 && adc < 3900)
        ){
         his->Fill(fineTS, adc);
      }
   }

   his->Draw();
}
