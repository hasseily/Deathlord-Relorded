/*
Program for Grid Cartographer to import tile IDs into a map
@RikRetro on Twitter
*/
function main( args )
{
	if (args.Length() == 1) {
		GCConsole.Print("Usage: "+args[0]+" TilesStartIndex XStart YStart \n");
		return;
	}
	var customTilesIndexStart = 0;
	var rowStart = 0;
	var colStart = 0;
	if (args.Length() > 1) {
		customTilesIndexStart = args[1].ToInt(10);
	}
	if (args.Length() == 4) {
		colStart = args[2].ToInt(10);
		rowStart = args[3].ToInt(10);
	}

	GCConsole.Print("Custom Tiles start index is: " + customTilesIndexStart + "\n");
	var buf = GCBuffer.Create();
	GCBuffer.Prepare(buf, 256000);
	var res = GCImport.BufferAs(buf, "Select csv file with a list of custom tile IDs to import", "CSV", "Comma-Delimited Files");
	if (res != GCOK) {
		GCConsole.Print("Error opening file: " + res + "\n");
		return;
	}
	GCKernel.SetBlockingMode(true);
	GCBuffer.Seek(buf, 0);
	var idTile = "00";
	var delim = ",";
	var r = rowStart;
	var c = colStart;
	while (1) {
		idTile = GCBuffer.ReadChars(buf, 2);
		if (idTile == null) {
			break;
		}
		GCTile.Select(c,r);
		GCTile.Set({ Terrain = {Custom = (idTile.ToInt(16) + customTilesIndexStart)} });
		delim = GCBuffer.ReadChars(buf,1);
		if (delim == null) {
			break;
		}
		switch (delim) {
			case "," :
				{
					c = c+1;
					//GCConsole.Print(c+" , "+r+"\n");
					break;
				};
			case "\r" :
					delim = GCBuffer.ReadChars(buf,1);
			case "\n" :
				{
					r = r+1;
					c = colStart;
					GCConsole.Print(".");
					break;
				};
		}
	}
	GCConsole.Print("\n");
	GCKernel.SetBlockingMode(false);
}