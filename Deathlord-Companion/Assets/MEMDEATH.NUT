
/*

MIT License

Copyright (c) 2020 David Walters

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/*
Deathlord version of Memory Analyzer
by Rikkles
@RikRetro on Twitter

You can activate the Deathlord character map by selecting it in the "Display" menu
*/

app_version <- "2.1";

STATUS_READY <- " Memory Analyser for Deathlord    Press ALT to activate menus";

include( "uikit/textmenu.nut" );
include( "uikit/displaybackup.nut" );
include( "uikit/textdialog.nut" );
include( "uikit/textform.nut" );
include( "uikit/textcursor.nut" );

DEATHLORD_CHARSET <- ["e","d","g","f","a","`","c","b","m","l","o","n","i","h","k","j","u","t","w","v","q","p","s","r"," "," ","]","[","y","x","*","z","E","D","G","F","A","@","C","B","M","L","O","N","I","H","K","J","U","T","W","V","Q","P","S","R"," "," "," "," ","Y","X","-","Z","%","$","'","&","!"," ","#","\"","-",",","/",".",")","(","+","*","5","4","7","6","1","0","3","2","=","<","?",">","9","8",";",":"];


function ParseHex( hex )
{
	var val = 0;

	hex = hex.Trim().ToLower();

	if ( hex == null || hex.Length() == 0 )
		return null;

	for ( var i = 0; i < hex.Length(); ++i )
	{
		var c = hex[ i ];
		var digit;
		if ( c >= '0' && c <= '9' )
			digit = c - '0';
		else if ( c >= 'a' && c <= 'f' )
			digit = c - 'a' + 10;
		else
			return null;

		val *= 16;
		val += digit;
	}

	return val;
}

editor <-
{
	snapBuffer = 0,

	tagBuffer = 0,
	tagCount = 0,

	cursorRow = 0,
	cursorCol = 0,

	rowStartsPruned = [],

	baseRow = 0,
	memRows = 0,
	scrollPip = -1,

	edit_mode = false,
	new_data = 0,
	old_data = 0,
	edit_cursor = 0,

	show_all_bytes = true,
	hide_rows = true,
	byte_group = 1,
	display_hex = true,
	display_signed = true,
	big_endian = true,
	high_ascii = false,

	status = "",
};

function editor::SetTag( addr )
{
	GCBuffer.Seek( editor.tagBuffer, addr / 8 );
	var tagBits = GCBuffer.Peek8( editor.tagBuffer );
	var addrBit = 1 << ( addr & 0x7 );

	if ( tagBits & addrBit )
	{
		// nothing to do.
	}
	else
	{
		++tagCount;
		tagBits = tagBits | addrBit;
		GCBuffer.Write8( editor.tagBuffer, tagBits );
	}
}

function editor::ClearTag( addr )
{
	GCBuffer.Seek( editor.tagBuffer, addr / 8 );
	var tagBits = GCBuffer.Peek8( editor.tagBuffer );
	var addrBit = 1 << ( addr & 0x7 );

	if ( tagBits & addrBit )
	{
		--tagCount;
		tagBits = tagBits & ~addrBit;
		GCBuffer.Write8( editor.tagBuffer, tagBits );
	}
	else
	{
		// nothing to do.
	}
}

function editor::Goto( addr )
{
	if ( addr == null )
		return;

	cursorCol = addr & 15;
	var para = addr & ~(15);

	for ( var row = 0; row < Rows(); ++row )
	{
		var s = RowAddr( row );

		if ( s > para )
		{
			onAddrChange();
			break;
		}
		else
		{
			baseRow = row - 10;
			cursorRow = row;
		}
	}
}

function editor::IsRowHidden( addr )
{
	if ( tagCount == 0 )
		return -1;

	// read tags
	GCBuffer.Seek( editor.tagBuffer, ( addr / 16 ) * 2 );
	var tags = GCBuffer.Read16LE( editor.tagBuffer );

	if ( tags == 0 )
	{
		return true;
	}
	else
	{
		return false;
	}
}

function editor::Rows()
{
	if ( hide_rows && rowStartsPruned.Length() > 0 )
		return rowStartsPruned.Length();
	else
		return memRows;
}

function editor::RowAddr( row )
{
	if ( hide_rows )
	{
		if ( row >= 0 && row < rowStartsPruned.Length() )
		{
			return rowStartsPruned[ row ];
		}
		else
		{
			return null;
		}
	}
	else
	{
		return row * 16;
	}
}

function editor::CursorAddr()
{
	var row = RowAddr( cursorRow );

	if ( row == null )
		return null;
	else
		return row + cursorCol;
}

function editor::ResetSnap()
{
	// Make all rows visible
	rowStartsPruned = [];
	for ( var blk = 0; blk < memRows; ++blk )
	{
		rowStartsPruned.Add( blk * 16 );
	}

	// Set all bytes as tagged.
	GCBuffer.Seek( tagBuffer, 0 );
	GCBuffer.Fill( tagBuffer, 0xFF, memRows * 2 );
	tagCount = memRows * 16;

	// Take an up-to-date memory snapshot.
	GCBuffer.Seek( snapBuffer, 0 );
	GCGameLink.MemoryCopy( snapBuffer, 0, memRows * 16 );
}

function editor::onStateChange()
{
	baseRow = 0;
	cursorRow = 0;
	cursorCol = 0;

	GCBuffer.Reset( tagBuffer );
	tagCount = 0;
	GCBuffer.Reset( snapBuffer );

	rowStartsPruned = [];

	var memsize = GCGameLink.MemorySize();
	var memlimit = 8 * 1024*1024;

	// dosbox hack
	var corename = GCGameLink.CoreName();
	if ( corename != null )
	{
		corename = corename.ToLower();
		if ( corename.Find( "dosbox" ) != null )
			memlimit = 640 * 1024;
	}

	if ( memsize > memlimit )
		memsize = memlimit;

	memRows = memsize / 16;
	if ( memRows > 0 )
	{
		scrollPip = 0;

		DrawBlankScreen();
		editor.status = "Please Wait ...";
		DrawStatus();

		editor.ResetSnap();
	}
	else
	{
		scrollPip = -1;
	}

	editor.status = STATUS_READY;
	DrawStatus();
}

function editor::WriteData( data )
{
	switch ( byte_group )
	{

	case 1:
		GCGameLink.Write8( editor.CursorAddr(), data );
		break;

	case 2:
		if ( big_endian )
		{
			GCGameLink.Write8( editor.CursorAddr(), ( data >> 8 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 1, ( data & 0xFF ) );
		}
		else
		{
			GCGameLink.Write8( editor.CursorAddr() + 1, ( data >> 8 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr(), ( data & 0xFF ) );
		}
		break;

	case 4:
		if ( big_endian )
		{
			GCGameLink.Write8( editor.CursorAddr(), ( data >> 24 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 1, ( data >> 16 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 2, ( data >> 8 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 3, ( data ) & 0xFF );
		}
		else
		{
			GCGameLink.Write8( editor.CursorAddr() + 3, ( data >> 24 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 2, ( data >> 16 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr() + 1, ( data >> 8 ) & 0xFF );
			GCGameLink.Write8( editor.CursorAddr(), ( data ) & 0xFF );
		}
		break;

	}
}

function editor::ReadData()
{
	var data;

	switch ( byte_group )
	{

	case 1:
		data = GCGameLink.Read8( editor.CursorAddr() );
		break;

	case 2:
		if ( big_endian )
		{
			data = GCGameLink.Read16BE( editor.CursorAddr() );
		}
		else
		{
			data = GCGameLink.Read16LE( editor.CursorAddr() );
		}
		break;

	case 4:
		if ( big_endian )
		{
			data = GCGameLink.Read16BE( editor.CursorAddr() ) << 16;
			data = data | GCGameLink.Read16BE( editor.CursorAddr() + 2 );
		}
		else
		{
			data = GCGameLink.Read16LE( editor.CursorAddr() + 2 ) << 16;
			data = data | GCGameLink.Read16LE( editor.CursorAddr() );
		}
		break;

	}

	return data;
}

function editor::BeginEditMemory()
{
	GCConsole.CursorEnable( false );

	DrawRAM();

	UITextCursor.Hide();

	// Move cursor to first character of current data byte
	var cx = 12 + ( cursorCol * 3 );
	GCConsole.Locate( cx, cursorRow - baseRow + 2 );

	// capture the data at this address
	old_data = ReadData();

	GCConsole.Attrib( display.backAttr );
	print( hexfmt( old_data, byte_group * 2 ) );
	GCConsole.Locate( cx, cursorRow - baseRow + 2 );

	edit_cursor = 0;
	new_data = old_data;

	GCConsole.CursorEnable( true );
	UITextCursor.Show();
}

function editor::FlushEdit()
{
	if ( edit_mode )
	{
		// flush
		if ( new_data != old_data )
		{
			WriteData( new_data );
		}
	}
}

function editor::DoEditMemory()
{
	if ( GCGameLink.IsActive() == false )
	{
		no_GCGameLink();
		return;
	}

	// Supported?
	if ( !( "Write8" in GCGameLink ) )
	{
		UITextDialog.MessageBox( "Edit mode is not supported by this\nversion of Grid Cartographer.\n\nPlease update to a more recent version." );
		return;
	}

	edit_mode = true;

	editor.status = " Edit Mode    Press ESC to cancel";
	DrawStatus();

	BeginEditMemory();

	while ( edit_mode )
	{
		if ( GCGameLink.IsActive() == false )
		{
			edit_mode = false;
			break;
		}

		var ch = GCConsole.PeekChar();
		if ( ch == GCKey.Esc )
		{
			edit_mode = false;
			break;
		}
		else if ( ch == GCKey.Enter || ch == GCKey.F2 )
		{
			FlushEdit();
			edit_mode = false;
			break;
		}
		else if ( ch == GCKey.Home ||
				  ch == GCKey.End ||
				  ch == GCKey.PageUp ||
				  ch == GCKey.PageDown ||
				  ch == GCKey.Left ||
				  ch == GCKey.Right ||
				  ch == GCKey.Up ||
				  ch == GCKey.Down )
		{
			FlushEdit();
			onKey( ch );
			continue;
		}

		// valid digit?
		var digit = null;
		if ( ch >= '0' && ch <= '9' )
			digit = ch - '0';
		else if ( ch >= 'a' && ch <= 'f' )
		{
			ch = ch - 'a' + 'A';
			digit = ch - 'A' + 10;
		}
		else if ( ch >= 'A' && ch <= 'F' )
			digit = ch - 'A' + 10;

		if ( digit != null )
		{
			putchar( ch );

			var nibble = ( byte_group * 2 ) - ( edit_cursor + 1 );

			new_data = new_data & ~( ( 0xF ) << ( nibble * 4 ) );
			new_data = new_data | ( ( digit & 0xF ) << ( nibble * 4 ) );

			++edit_cursor;

			if ( edit_cursor >= byte_group * 2 )
			{
				FlushEdit();
				onKey( GCKey.Right ); // next value
				break;
			}
		}

		var pointer = GCConsole.Pointer();
		UpdateMouse( pointer );

		if ( menu.Update() )
		{
			FlushEdit();
			edit_mode = false;
			break;
		}
		else
		{
			TickMouse( pointer );

			UITextCursor.Update();
		}
	}

	GCConsole.CursorEnable( false );

	editor.status = STATUS_READY;
	DrawStatus();

	return edit_mode;
}

//------------------------------


function updateDisplayMenu()
{
	{
		var opts = menu.Menus[ g_display_menu ].options;

		opts[ 0 ].checked <- ( editor.show_all_bytes == false );
		opts[ 2 ].checked <- ( editor.hide_rows == true );
		opts[ 4 ].checked <- ( editor.byte_group == 1 );
		opts[ 5 ].checked <- ( editor.byte_group == 2 );
		opts[ 6 ].checked <- ( editor.byte_group == 4 );
		opts[ 8 ].checked <- ( editor.big_endian );
		opts[ 10 ].checked <- ( editor.high_ascii );
	}

	onAddrChange();
}

function countBits( val, max )
{
	var count = 0;

	for ( var i = 0; i < max; ++i )
	{
		var bit = 1 << i;
		if ( val & bit )
			++count;
	}

	return count;
}

function process( fn )
{
	editor.status = "Processing ...";
	DrawStatus();

	for ( var row = 0; row < editor.rowStartsPruned.Length(); /**/ )
	{
		var block_addr = editor.rowStartsPruned[ row ];
		var rdfn;

		// Get old values
		var o = [];
		GCBuffer.Seek( editor.snapBuffer, block_addr );
		switch ( editor.byte_group )
		{
		case 1:
			for ( var ibyte = 0; ibyte < 16; ++ibyte )
				o.Add( GCBuffer.Read8( editor.snapBuffer ) );
			break;
		case 2:
			rdfn = editor.big_endian ? GCBuffer.Read16BE : GCBuffer.Read16LE;
			for ( var iword = 0; iword < 8; ++iword )
				o.Add( rdfn( editor.snapBuffer ) );
			break;
		case 4:
			rdfn = editor.big_endian ? GCBuffer.Read32BE : GCBuffer.Read32LE;
			for ( var idword = 0; idword < 4; ++idword )
				o.Add( rdfn( editor.snapBuffer ) );
			break;
		}

		// Update
		GCBuffer.Seek( editor.snapBuffer, block_addr );
		GCGameLink.MemoryCopy( editor.snapBuffer, block_addr, 16 );

		// Get new values.
		var n = [];
		GCBuffer.Seek( editor.snapBuffer, block_addr );
		switch ( editor.byte_group )
		{
		case 1:
			for ( var ibyte = 0; ibyte < 16; ++ibyte )
				n.Add( GCBuffer.Read8( editor.snapBuffer ) );
			break;
		case 2:
			rdfn = editor.big_endian ? GCBuffer.Read16BE : GCBuffer.Read16LE;
			for ( var iword = 0; iword < 8; ++iword )
				n.Add( rdfn( editor.snapBuffer ) );
			break;
		case 4:
			rdfn = editor.big_endian ? GCBuffer.Read32BE : GCBuffer.Read32LE;
			for ( var idword = 0; idword < 4; ++idword )
				n.Add( rdfn( editor.snapBuffer ) );
			break;
		}

		// read tags
		GCBuffer.Seek( editor.tagBuffer, block_addr / 8 );
		var tags = GCBuffer.Peek16( editor.tagBuffer );

		// uncount tag bits
		editor.tagCount -= countBits( tags, 16 );

		var group = ( 1 << editor.byte_group ) - 1;

		// Compare
		var data_max = 16 / editor.byte_group;
		for ( var idata = 0; idata < data_max; ++idata )
		{
			var tag_bits = group << ( idata * editor.byte_group );

			if ( tags & tag_bits )
			{
				if ( fn( o[idata], n[idata] ) == false ) // <-- old,new
				{
					// Clear tag.
					tags = tags & ~tag_bits;
				}
			}
		}

		// re-count tag bits
		editor.tagCount += countBits( tags, 16 );

		// write tags
		GCBuffer.Write16( editor.tagBuffer, tags );

		// Delete row?
		if ( tags == 0 )
		{
			editor.rowStartsPruned.RemoveAt( row );
		}
		else
		{
			++row;
		}
	}

	/*if ( editor.tagCount == 0 )
	{
		editor.ResetSnap();

		editor.status = "0 results. Restarting.";
	}
	else*/
	{
		editor.status = editor.tagCount + " results.";
	}

	DrawStatus();
	onAddrChange();
}

function hexfmt( _val, _len )
{
	if ( _val == null )
		return "NULL";
	var s = _val.ToHexStr();
	s = s.ToUpper();
	while ( s.Length() < _len )
		s = "0" + s;
	return s;
}

function decfmt( _val, _len )
{
	if ( _val == null )
		return "NULL";
	var s = _val.ToString();
	while ( s.Length() < _len )
		s = " " + s;
	return s;
}

function initMenu()
{
	GCConsole.SetCaption( "Deathlord Memory Analyser" );

	menu.Menus = [];

	menu.Menus.Add({
		name = "&File",
		options =
		[
			{ name = "&Export RAM...", id = "file_export" },
			{ div=1 },
			{ name = "E&xit", id = "file_exit" },
		]
	});

	g_edit_menu <- menu.Menus.Length();

	menu.Menus.Add({
		name = "&Edit",
		options =
		[
			{ name = "&Edit Mode...", id = "edit_edit", shortcut="F2" },
		]
	});

	menu.Menus.Add({
		name = "&Search",
		options =
		[
			{ name = "&Goto Address...", id = "search_goto", shortcut="Ctrl+G" },
		]
	});

	menu.Menus.Add({
		name = "Fi&lter",
		options =
		[
			{ name = "&Restart", id = "tag_reset", shortcut="F5" },
			{ div=1 },
			{ name = "now == old", id = "tag_EQ", shortcut="1" },
			{ name = "now != old", id = "tag_NEQ", shortcut="2" },
			{ name = "now <  old", id = "tag_LT", shortcut="3" },
			{ name = "now <= old", id = "tag_LTE", shortcut="4" },
			{ name = "now >  old", id = "tag_GT", shortcut="5" },
			{ name = "now >= old", id = "tag_GTE", shortcut="6" },
			{ name = "now == 0", id = "tag_EQ0", shortcut="7" },
			{ name = "now != 0", id = "tag_NEQ0", shortcut="8" },
			{ div=1 },
			{ name = "Specific Value...", id = "tag_EQVAL", shortcut="9" },
			{ div=1 },
			{ name = "&Toggle Tag", id = "tag_toggle", shortcut="SPACE" },
		]
	});

	g_display_menu <- menu.Menus.Length();

	menu.Menus.Add({
		name = "&Display",
		options =
		[
			{ name = "&Hide Untagged", id = "display_tagged", shortcut="H" },
			{ div=1 },
			{ name = "&Collapse Rows", id = "tag_hide_rows" },
			{ div=1 },
			{ name = "&1-Byte", id = "display_1byte" },
			{ name = "&2-Byte", id = "display_2byte" },
			{ name = "&4-Byte", id = "display_4byte" },
			{ div=1 },
			{ name = "Big &Endian", id = "display_endian" },
			{ div=1 },
			{ name = "&Deathlord CHAR Map", id = "display_highascii" },
		]
	});

	menu.Menus.Add({
		name = "De&bug",
		options =
		[
			{ name = "Pause", id = "debug_pause" },
			{ name = "Resume", id = "debug_resume" },
		]
	});

	menu.Menus.Add({
		name = "&Help",
		right = true,
		options = [
			{ name = "Content &Hash...", id = "help_hash" },
			{ div=1 },
			{ name = "&About...", id = "help_about", shortcut="F1" },
		]
	});

	updateDisplayMenu();
}

function DrawData_1( addr, a, tags, cursorAddr )
{
	foreach( index_el, el in a )
	{
		putchar( ' ' );

		var addrByte = addr + index_el;
		var addrBit = 1 << ( addrByte & 0xF );
		var tagged = tags & addrBit;

		if ( ( !editor.show_all_bytes && !tagged ) )
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( display.invAttr );

			print( ".." );
		}
		else
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( tagged ? display.invMarkAttr : display.invAttr );
			else if ( tagged )
				GCConsole.Attrib( display.markAttr );

			if ( el == null )
			{
				print( "??" );
			}
			else
			{
				print( hexfmt(el,2) );
			}
		}

		GCConsole.Attrib( display.backAttr );
	}

	print( "  " );
}

function DrawData_2( addr, a, tags, cursorAddr )
{
	var el;

	for ( var index_el = 0; index_el < 16; index_el += 2 )
	{
		if ( editor.big_endian )
			el = a[ index_el + 1 ] | ( a[ index_el ] << 8 );
		else
			el = a[ index_el ] | ( a[ index_el + 1 ] << 8 );

		putchar( ' ' );

		var addrByte = addr + index_el;
		var addrBit = 3 << ( addrByte & 0xF );
		var tagged = tags & addrBit;

		if ( ( !editor.show_all_bytes && !tagged ) )
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( display.invAttr );

			print( "...." );
		}
		else
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( tagged ? display.invMarkAttr : display.invAttr );
			else if ( tagged )
				GCConsole.Attrib( display.markAttr );

			if ( el == null )
			{
				print( "????" );
			}
			else
			{
				print( hexfmt( el, 4 ) );
			}
		}

		GCConsole.Attrib( display.backAttr );

		putchar( ' ' );
	}

	print( "  " );
}

function DrawData_4( addr, a, tags, cursorAddr )
{
	var el;

	for ( var index_el = 0; index_el < 16; index_el += 4 )
	{
		if ( editor.big_endian )
			el = a[ index_el + 3 ]
				| ( a[ index_el + 2 ] << 8 )
				| ( a[ index_el + 1 ] << 16 )
				| ( a[ index_el ] << 24 );
		else
			el = a[ index_el ]
				| ( a[ index_el + 1 ] << 8 )
				| ( a[ index_el + 2 ] << 16 )
				| ( a[ index_el + 3 ] << 24 );

		putchar( ' ' );

		var addrByte = addr + index_el;
		var addrBit = 15 << ( addrByte & 0xF );
		var tagged = tags & addrBit;

		if ( ( !editor.show_all_bytes && !tagged ) )
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( display.invAttr );

			print( "........" );
		}
		else
		{
			if ( cursorAddr == addrByte )
				GCConsole.Attrib( tagged ? display.invMarkAttr : display.invAttr );
			else if ( tagged )
				GCConsole.Attrib( display.markAttr );

			if ( el == null )
			{
				print( "????????" );
			}
			else
			{
				print( hexfmt( el, 8 ) );
			}
		}

		GCConsole.Attrib( display.backAttr );

		print( "   " );
	}

	print( "  " );
}

function DrawRAM()
{
	UITextCursor.Hide();

	var row;
	var rowMax = editor.Rows();

	for ( row = 0; row < rowMax; ++row )
	{
		// end of the screen?
		if ( row >= display.viewHeight )
			break;

		var addr = editor.RowAddr( editor.baseRow + row );
		if ( addr == null )
			break;

		var cursorAddr = editor.CursorAddr();

		GCConsole.Locate( 1, row + 2 );
		GCConsole.Attrib( display.backAttr );

		print( " " );
		print( hexfmt( addr & 0xFFFFFF, 6 ) );
		print( " : " );

		// read tags
		GCBuffer.Seek( editor.tagBuffer, addr / 8 );
		var tags = GCBuffer.Read16LE( editor.tagBuffer );

		var a = GCGameLink.ReadBlock8( addr, 16 );

		switch ( editor.byte_group )
		{
		case 1:
			DrawData_1( addr, a, tags, cursorAddr );
			break;
		case 2:
			DrawData_2( addr, a, tags, cursorAddr );
			break;
		case 4:
			DrawData_4( addr, a, tags, cursorAddr );
			break;
		}

		var cursorMask = ~( editor.byte_group - 1 );

		foreach( index_el, el in a )
		{
			var addrByte = addr + index_el;
			var addrBit = 1 << ( addrByte & 0xF );
			var tagged = tags & ( 1 << index_el );

			if ( ( !editor.show_all_bytes && !tagged ) )
			{
				if ( cursorAddr == addr + index_el )
					GCConsole.Attrib( display.invAttr );

				putchar( '.' );
			}
			else
			{
				if ( ( cursorAddr & cursorMask ) == ( ( addr + index_el ) & cursorMask ) )
					GCConsole.Attrib( tagged ? display.invMarkAttr : display.invAttr );
				else if ( tagged )
					GCConsole.Attrib( display.markAttr );

				if ( el == null )
				{
					putchar( '?' );
				}
				else if ( !editor.high_ascii && ( el < 32 ||  el > 127 ) )
				{
					putchar( '.' );
				}
				else if ( editor.high_ascii && ( el > 223 ) )
				{
					putchar( '.' );
				}
				else if ( editor.high_ascii && ( el > 95 && el < 128) )
				{
					putchar( '~' );
				}
				else
				{
					var s;
					/* 
					Deathlord has its own charmap that starts at 0 and ends at 5F.
					The high ascii bit is set to specify an end-of-string character
					*/
					if ( editor.high_ascii )
					{
						var el2 = el;
						if (el > 127)
						{
							el2 = el2 - 128;
						}
						s = DEATHLORD_CHARSET[el2];
					}
					else
					{
					 s = el.ToChar()
					}
					print( s );
				}
			}

			GCConsole.Attrib( display.backAttr );
		}

		print( "  " );
	}

	// .. blank remaning rows
	GCConsole.Attrib( display.backAttr );
	for ( ; row < display.viewHeight; ++row )
	{
		GCConsole.Locate( 1, row + 2 );
		for ( var i = 0; i < 78; ++i )
			putchar(' ');
	}

	UITextCursor.Show();
}

function DrawStatus()
{
	var liteAttr;
	var darkAttr;

	switch ( adapter.type )
	{

	case "HERC":
		liteAttr = 0x70;
		darkAttr = 0x70;
		break;

	default:
		liteAttr = 0x3F;
		darkAttr = 0x30;
		break;

	}

	// background
	var addr = display.baseAddr + ( ( adapter.rows - 1 ) * adapter.columns * 2 );
	GCConsole.Write8( addr, liteAttr );
	for ( var x = 0; x < adapter.columns; ++x )
		GCConsole.Write8( addr + 1 + x * 2, liteAttr );

	GCConsole.Attrib( liteAttr );
	GCConsole.Locate( 0, adapter.rows - 1 );

	putchar(' ');
	print( editor.status );

	while ( GCConsole.Column() < adapter.columns - 18 )
		putchar(' ');

	GCConsole.Attrib( darkAttr );
	GCConsole.Locate( adapter.columns - 23, adapter.rows - 1 );

	if ( GCGameLink.IsActive() )
	{
		putchar( 0xB3 );

		GCConsole.Locate( adapter.columns - 21, adapter.rows - 1 );

		print( decfmt( editor.tagCount, 9 ) );

		GCConsole.Locate( adapter.columns - 11, adapter.rows - 1 );

		putchar( 0xB3 );

		GCConsole.Locate( adapter.columns - 9, adapter.rows - 1 );

		var pos = editor.CursorAddr();

		if ( pos == null )
		{
			print( "??????h" );
		}
		else
		{
			print( hexfmt( pos & 0xFFFFFF, 6 ) + "h" );
		}

	}

	GCConsole.Attrib( display.attr );
}

function DrawScrollV( y0, y1, pip )
{
	GCConsole.Attrib( display.invAttr );

	GCConsole.Locate( adapter.columns - 1, y0 );
	putchar( 0x18 );

	GCConsole.Locate( adapter.columns - 1, y1 - 1 );
	putchar( 0x19 );

	GCConsole.Attrib( display.attr );

	for ( var y = y0 + 1; y < y1 - 1; ++y )
	{
		GCConsole.Locate( adapter.columns - 1, y );
		putchar( 0xB2 );
	}

	if ( pip >= 0 )
	{
		GCConsole.Locate( adapter.columns - 1, y0 + 1 + pip );
		putchar( ' ' );
	}
}

function DrawWindowCaption()
{
	GCConsole.Locate( 0, 1 );

	var coreName = GCGameLink.CoreName();
	if ( coreName == null )
		coreName = "unknown";
	var windowCaption = coreName.ToUpper() + " [";
	if ( editor.memRows >= 64 /*1KB*/ )
		windowCaption += ( editor.memRows / 64 ) + "KB]";
	else if ( editor.memRows == 0 /*N/A*/ )
		windowCaption += "N/A]";
	else
		windowCaption += ( editor.memRows * 16 ) + "B]";

	if ( GCGameLink.IsPaused() )
		windowCaption += " - PAUSED";

	// caption
	var cap_x = ( adapter.columns - ( windowCaption.Length() + 2 ) ) / 2;

	GCConsole.Attrib( display.backAttr );
	GCConsole.Locate( 0, 1 );
	putchar( 0xDA );

	for ( var x = 1; x < cap_x; ++x )
		putchar( 0xC4 );

	GCConsole.Attrib( display.invAttr );
	putchar( ' ' );
	print( windowCaption );
	putchar( ' ' );
	GCConsole.Attrib( display.backAttr );

	for ( var x = GCConsole.Column(); x < adapter.columns - 1; ++x )
		putchar( 0xC4 );

	putchar( 0xBF );
}

function DrawWindow()
{
	DrawWindowCaption();

	// sides
	GCConsole.Attrib( display.backAttr );
	for ( var y = 2; y < adapter.rows - 1; ++y )
	{
		GCConsole.Locate( 0, y );
		putchar( 0xB3 );
		GCConsole.Locate( adapter.columns - 1, y );
		putchar( 0xB3 );
	}

	// scroll bar
	DrawScrollV( 2, adapter.rows - 1, editor.scrollPip );
}

function DrawBlankScreen()
{
	GCConsole.Clear();

	var b;
	if ( display.is_herc )
		b = 0x07B0;
	else
		b = 0x1720;

	// background
	var addr = display.baseAddr + ( adapter.columns * 2 );
	var area = ( adapter.rows - 1 ) * adapter.columns;
	for ( var x = 0; x < area; ++x )
		GCConsole.Write16( addr + x * 2, b );

	menu.Refresh();

	editor.status = "Waiting...";
	DrawStatus();
}

function DrawScreen()
{
	GCConsole.Clear();

	DrawWindow();

	DrawStatus();

	menu.Refresh();
}

function init()
{
	backup <- UIDisplayBackup();
	backup.Store();

	adapter <- GCConsole.Adapter();

	if ( adapter.columns == 40 )
	{
		// switch to 80 columns mode.
		GCConsole.Mode( 3 );
		adapter <- GCConsole.Adapter();
	}

	display <- {};

	mouse <-
	{
		click = 0,
		old_b = 0,
		hold_limit = 5, // slow
		hold_time = 0
	};

	GCConsole.CursorEnable( false );

	menu <- UITextMenu();

	switch ( adapter.type )
	{

	case "HERC":
		display.is_herc <- true;
		display.baseAddr <- 0xB0000;
		display.backAttr <- 0x07;
		display.attr <- 0x07;
		display.invAttr <- 0x70;
		display.markAttr <- 0x0F;
		display.invMarkAttr <- 0x78;
		break;

	default:
		display.is_herc <- false;
		display.baseAddr <- 0xB8000;
		display.backAttr <- 0x17;
		display.attr <- 0x07;
		display.invAttr <- 0x70;
		display.markAttr <- 0x1A;
		display.invMarkAttr <- 0x21;
		break;

	}

	display.viewHeight <- ( adapter.rows - 3 );

	initMenu();
}

function exit()
{
	backup.Restore();
	GCKernel.Exit();
}

function no_GCGameLink()
{
	UITextDialog.MessageBox( "Game Link is not running." );
}

function no_snap()
{
	UITextDialog.MessageBox( "Snap buffer is empty." );
}

function onMenuSelect( id )
{
	switch ( id )
	{

	case "file_export":

		if ( GCGameLink.IsActive() )
		{
			editor.status = "Please Wait ...";
			DrawStatus();

			var content_name = GCGameLink.ContentName();

			var buffer = GCBuffer.Create();

			// Take an up-to-date memory snapshot.
			GCGameLink.MemoryCopy( buffer, 0, -1 );

			// Export!
			GCExport.BufferAs( buffer, "Save RAM...", content_name, "bin", "Memory Dump" );

			GCBuffer.Destroy( buffer );

			editor.status = "Export Complete.";
			DrawStatus();
		}
		else
		{
			no_GCGameLink();
		}

		break;

	case "edit_edit":
		while (	editor.DoEditMemory() )
		{
			continue;
		}
		editor.edit_mode = false;
		break;

	case "debug_pause":
		GCGameLink.Pause();
		break;

	case "debug_resume":
		GCGameLink.Resume();
		break;

	case "help_hash":
		{
			var text;
			var content_hash;
			var options;

			if ( GCGameLink.IsActive() )
			{
				var content_name = GCGameLink.ContentName();
				var content_hash = GCGameLink.ContentHash();

				if ( content_name == null || content_hash == null )
				{
					UITextDialog.MessageBox( "Not available for this system." );
				}
				else
				{
					text = content_hash.Slice(0,16)+"-"+
						   content_hash.Slice(16,32)+"\n"+
						   content_hash.Slice(32,48)+"-"+
						   content_hash.Slice(48,64);

					var dlg = {
						id = "help_hash",
						caption = "Content Hash",
						width = 37,
						height = 4,
						form =
						[
							UITextForm.Label( { X = 2, Y = 1, Text = text } )
						],
						OnSelectOption = function( sender, selection )
						{
							if ( selection.index == 0 )
							{
								// Write it to a memory buffer.
								var buffer = GCBuffer.Create();
								GCBuffer.Print( buffer, content_hash );

								// Export!
								GCExport.BufferAs( buffer, "Export Hash...", content_name, "txt", "Text File" );

								GCBuffer.Destroy( buffer );
							}

							sender.EndDialog(1);
						},
						options = [
							{ space="7", text = " Export " },
							{ space="3", text = " Cancel " }
						]
					};

					UITextDialog( dlg ).OpenDialog();
				}
			}
			else
			{
				no_GCGameLink();
			}
		}
		break;

	case "file_exit":
		exit();
		break;

	case "search_goto":

		if ( GCGameLink.IsActive() == false )
			break;

		{
			var pc = editor.CursorAddr();
			if ( pc == null )
				break;

			pc = pc.ToHexStr();

			var dlg = {
				caption = "Goto Address",
				width = 33,
				height = 3,
				form =
				{
					label1 = UITextForm.Label( { X = 2, Y = 1, Text = "Address (Hex):" } ),
					editBox1 = UITextForm.TextBox( { X = 18, Y = 0, MaxLength = 6, Width = 10,
						TabIndex = 1, Text = pc } )
				},
				OnSelectOption = function( sender, selection )
				{
					if ( selection.index == 0 )
					{
						var val = form[ "editBox1" ].Text;
						sender.EndDialog( val );
					}
					else
					{
						sender.EndDialog( "cancel" );
					}
				},
				options = [
					{ space="9", text = " OK " },
					{ space="4", text = " Cancel " }
				]
			};

			var hex = UITextDialog( dlg ).OpenDialog();

			if ( hex != "cancel" )
			{
				var addr = ParseHex( hex );

				if ( addr == null || addr < 0 || addr > 0xFFFFFF )
				{
					UITextDialog.MessageBox( "\n  Must specify a hex value.  \n" );

					onMenuSelect( "search_goto" );
				}
				else if ( addr >= editor.memRows * 16 )
				{
					UITextDialog.MessageBox( "\n  Address is too large.  \n" );

					onMenuSelect( "search_goto" );
				}
				else
				{
					editor.Goto( addr );
				}
			}
		}

		break;

	case "tag_snap":
		if ( GCGameLink.IsActive() )
		{
			editor.ResetSnap();
		}
		else
		{
			no_GCGameLink();
		}
		break;

	case "tag_EQ":
		process( @(old,new) new == old );
		break;

	case "tag_NEQ":
		process( @(old,new) new != old );
		break;

	case "tag_LT":
		process( @(old,new) new < old );
		break;

	case "tag_LTE":
		process( @(old,new) new <= old );
		break;

	case "tag_GT":
		process( @(old,new) new > old );
		break;

	case "tag_GTE":
		process( @(old,new) new >= old );
		break;

	case "tag_EQ0":
		process( @(old,new) new == 0 );
		break;

	case "tag_NEQ0":
		process( @(old,new) new != 0 );
		break;

	case "tag_EQVAL":

		if ( GCGameLink.IsActive() == false )
			break;

		{
			var char_width;

			switch ( editor.byte_group )
			{
			case 1:
				char_width = 2;
				break;
			case 2:
				char_width = 4;
				break;
			case 4:
				char_width = 8
				break;
			}

			var max_val;
			max_val = ( 1 << ( char_width * 4 ) ) - 1;

			var dlg = {
				id = "help_hash",
				caption = "Tag Specific Values",
				width = 37,
				height = 5,
				form =
				{
					label1 = UITextForm.Label( { X = 7, Y = 2, Text = "Hex Value:" } ),
					editBox1 = UITextForm.TextBox( { MaxLength = char_width, X = 20, Y = 1, Width = char_width + 2, TabIndex = 1 } )
				},
				OnSelectOption = function( sender, selection )
				{
					if ( selection.index == 0 )
					{
						var val = form[ "editBox1" ].Text;
						sender.EndDialog( val );
					}
					else
					{
						sender.EndDialog( "cancel" );
					}
				},
				options = [
					{ space="9", text = " OK " },
					{ space="4", text = " Cancel " }
				]
			};

			var hex = UITextDialog( dlg ).OpenDialog();

			if ( hex != "cancel" )
			{
				var val = ParseHex( hex );

				if ( val == null || val < 0 || val > max_val )
				{
					UITextDialog.MessageBox( "\n   Must specify a hex value from 0 to " + hexfmt(max_val,char_width) + ".   \n" );

					onMenuSelect( "tag_EQVAL" );
				}
				else
				{
					process( @(old,new) new == val );
				}
			}
		}

		break;

	case "tag_clear":
		editor.ClearTag( editor.CursorAddr() );
		break;

	case "tag_toggle":
		{
			var addr = editor.CursorAddr();
			if ( addr == null )
				break;

			GCBuffer.Seek( editor.tagBuffer, addr / 8 );
			var tagBits = GCBuffer.Peek8( editor.tagBuffer );
			var group = ( 1 << editor.byte_group ) - 1;
			var addrBit = group << ( addr & 0x7 );

			if ( tagBits & addrBit )
			{
				var a = addr & ( ~( editor.byte_group - 1 ) );
				for ( var ai = 0; ai < editor.byte_group; ++ai )
					editor.ClearTag( a + ai );

				// Collapse row?
				if ( editor.IsRowHidden( addr ) )
				{
					editor.rowStartsPruned.RemoveAt( editor.cursorRow );
					onAddrChange();
				}
			}
			else
			{
				editor.SetTag( addr );
			}
		}
		break;

	case "tag_reset":

		if ( GCGameLink.IsActive() )
		{
			var reset = false;

			if ( editor.tagCount == 0 )
			{
				reset = true;
			}
			else
			{
				if ( UITextDialog.MessageBox( "\n  Are you sure? Previous results  \n  will be discarded.\n", ["OK","Cancel"] ) == "OK" )
				{
					reset = true;
				}
			}

			if ( reset )
			{
				editor.status = "Please Wait ...";
				DrawStatus();

				editor.show_all_bytes = true;
				editor.ResetSnap();
				editor.status = "Ready. Snapshot of " + GCBuffer.Length( editor.snapBuffer ) + " bytes.";
				DrawStatus();
				updateDisplayMenu();
			}
		}
		else
		{
			no_GCGameLink();
		}

		break;

	case "display_tagged":
		editor.show_all_bytes = !editor.show_all_bytes;
		updateDisplayMenu();
		break;

	case "tag_hide_rows":
		{
			var c = editor.CursorAddr();
			editor.hide_rows = !editor.hide_rows;
			editor.Goto( c );
			updateDisplayMenu();
		}
		break;

	case "display_1byte":
		editor.byte_group = 1;
		updateDisplayMenu();
		break;

	case "display_2byte":
		{
			editor.byte_group = 2;

			// fix address
			var cursorMask = ~( editor.byte_group - 1 );
			editor.cursorCol = editor.cursorCol & cursorMask;

			updateDisplayMenu();
		}
		break;

	case "display_4byte":
		{
			editor.byte_group = 4;

			// fix address
			var cursorMask = ~( editor.byte_group - 1 );
			editor.cursorCol = editor.cursorCol & cursorMask;

			updateDisplayMenu();
		}
		break;

	/*case "display_hex":
		editor.display_hex = !editor.display_hex;
		updateDisplayMenu();
		break;

	case "display_signed":
		editor.display_signed = !editor.display_signed;
		updateDisplayMenu();
		break;*/

	case "display_endian":
		editor.big_endian = !editor.big_endian;
		updateDisplayMenu();
		break;

	case "display_highascii":
		editor.high_ascii = !editor.high_ascii;
		updateDisplayMenu();
		break;

	case "help_about":

		UITextDialog.MessageBox( "     Memory Analyser" + "\n" +
								 "    by David Walters!   " + "\n\n" +
								 "       Version " + app_version + "      "
		);

		break;

	}
}

function onClickScroll( pointer )
{
	if ( pointer.y == 2 )
	{
		--editor.baseRow;

		// drag cursor along?
		if ( editor.cursorRow >= editor.baseRow + display.viewHeight )
			--editor.cursorRow;

		onAddrChange();
	}
	else if ( pointer.y == adapter.rows - 2 )
	{
		++editor.baseRow;

		// drag cursor along?
		if ( editor.cursorRow < editor.baseRow )
			++editor.cursorRow;

		onAddrChange();
	}
}

function onAddrChange()
{
	var rows = editor.Rows();
	var maxTopRow = rows - display.viewHeight;
	var oldBaseAddr = editor.baseRow;

	if ( editor.cursorRow < 0 )
		editor.cursorRow = 0;
	if ( editor.cursorRow >= rows )
		editor.cursorRow = rows - 1;

	if ( maxTopRow > 0 )
	{
		// push scroll
		while ( editor.cursorRow < editor.baseRow )
			--editor.baseRow;
		while ( editor.cursorRow >= editor.baseRow + display.viewHeight )
			++editor.baseRow;

		if ( editor.baseRow < 0 )
			editor.baseRow = 0;
		if ( editor.baseRow >= maxTopRow )
			editor.baseRow = maxTopRow;
	}
	else
	{
		editor.baseRow = 0;
	}

	// Scroll Bar

	var oldPip = editor.scrollPip;

	if ( maxTopRow <= 0 )
		editor.scrollPip = -1;
	else
		editor.scrollPip = editor.baseRow * ( adapter.rows - 6 ) / maxTopRow;

	if ( editor.scrollPip != oldPip )
	{
		DrawScrollV( 2, adapter.rows - 1, editor.scrollPip );
	}

	DrawStatus();

	if ( editor.edit_mode )
	{
		editor.BeginEditMemory();
	}
}

function TickKey()
{
	for ( ; ; )
	{
		var ch = GCConsole.PeekChar();
		if ( ch == 0 )
			break;

		onKey( ch );
	}
}

function onKey( ch )
{
	switch ( ch )
	{

	case '1':
		onMenuSelect( "tag_EQ" );
		break;

	case '2':
		onMenuSelect( "tag_NEQ" );
		break;

	case '3':
		onMenuSelect( "tag_LT" );
		break;

	case '4':
		onMenuSelect( "tag_LTE" );
		break;

	case '5':
		onMenuSelect( "tag_GT" );
		break;

	case '6':
		onMenuSelect( "tag_GTE" );
		break;

	case '7':
		onMenuSelect( "tag_EQ0" );
		break;

	case '8':
		onMenuSelect( "tag_NEQ0" );
		break;

	case '9':
		onMenuSelect( "tag_EQVAL" );
		break;

	case 'h':
		onMenuSelect( "display_tagged" );
		break;

	case 7: // ctrl+g
		onMenuSelect( "search_goto" );
		break;

	case GCKey.F1:
		onMenuSelect( "help_about" );
		break;

	case GCKey.F2:
		onMenuSelect( "edit_edit" );
		break;

	case GCKey.F5:
		onMenuSelect( "tag_reset" );
		break;

	case ' ':
		{
			onMenuSelect( "tag_toggle" );
			onKey( GCKey.Right );
		}
		break;

	case GCKey.Del:
		{
			onMenuSelect( "tag_clear" );
			onKey( GCKey.Right );
		}
		break;

	case GCKey.Back:
		{
			onKey( GCKey.Left );
			onMenuSelect( "tag_clear" );
		}
		break;

	case GCKey.Left:
		{
			editor.cursorCol -= editor.byte_group;
			if ( editor.cursorCol < 0 )
			{
				if ( editor.cursorRow > 0 )
				{
					var cursorMask = ~( editor.byte_group - 1 );
					editor.cursorCol = 15 & cursorMask;
					--editor.cursorRow;
				}
				else
				{
					editor.cursorCol = 0;
				}
			}

			onAddrChange();
		}
		break;

	case GCKey.Right:
		{
			editor.cursorCol += editor.byte_group;
			if ( editor.cursorCol > 15 )
			{
				if ( editor.cursorRow < editor.Rows() )
				{
					editor.cursorCol = 0;
					++editor.cursorRow;
				}
				else
				{
					var cursorMask = ~( editor.byte_group - 1 );
					editor.cursorCol = 15 & cursorMask;
				}
			}

			onAddrChange();
		}
		break;

	case GCKey.Up:
		{
			--editor.cursorRow;
			onAddrChange();
		}
		break;

	case GCKey.Down:
		{
			++editor.cursorRow;
			onAddrChange();
		}
		break;

	case GCKey.PageUp:
		{
			editor.cursorRow -= display.viewHeight;
			editor.baseRow -= display.viewHeight;
			onAddrChange();
		}
		break;

	case GCKey.PageDown:
		{
			editor.cursorRow += display.viewHeight;
			editor.baseRow += display.viewHeight;
			onAddrChange();
		}
		break;

	case GCKey.Home:
		{
			editor.cursorCol = 0;
			onAddrChange();
		}
		break;

	case GCKey.End:
		{
			var cursorMask = ~( editor.byte_group - 1 );
			editor.cursorCol = 0xF & cursorMask;
			onAddrChange();
		}
		break;

	default:
		return;

	}
}

function TickMouse( pointer )
{
	if ( pointer == null )
		return;

	// Left Click?
	if ( mouse.click == 1 )
	{
		if ( pointer.x == adapter.columns - 1 )
		{
			editor.FlushEdit();

			onClickScroll( pointer );
		}

		if ( pointer.y >= 2 && pointer.y < adapter.rows - 1 )
		{
			if ( pointer.x >= 61 && pointer.x <= 76 )
			{
				editor.FlushEdit();

				// text area
				editor.cursorRow = editor.baseRow + ( pointer.y - 2 );
				editor.cursorCol = pointer.x - 61;

				var cursorMask = ~( editor.byte_group - 1 );
				editor.cursorCol = editor.cursorCol & cursorMask;

				onAddrChange();
				return;
			}

			switch ( editor.byte_group )
			{

			case 1:

				if ( pointer.x >= 12 && pointer.x <= 58 )
				{
					editor.FlushEdit();

					// value area
					editor.cursorRow = editor.baseRow + ( pointer.y - 2 );
					editor.cursorCol = ( pointer.x - 12 ) / 3;

					onAddrChange();
				}

				break;

			case 2:

				if ( pointer.x >= 12 && pointer.x <= 57 )
				{
					var d = ( pointer.x - 12 ) / 6;
					var o = ( pointer.x - 12 ) % 6;

					if ( o < 4 )
					{
						editor.FlushEdit();

						// value area
						editor.cursorRow = editor.baseRow + ( pointer.y - 2 );
						editor.cursorCol = 2 * d;

						onAddrChange();
					}
				}

				break;

			case 4:

				if ( pointer.x >= 12 && pointer.x <= 55 )
				{
					var d = ( pointer.x - 12 ) / 12;
					var o = ( pointer.x - 12 ) % 12;

					if ( o < 8 )
					{
						editor.FlushEdit();

						// value area
						editor.cursorRow = editor.baseRow + ( pointer.y - 2 );
						editor.cursorCol = 4 * d;

						onAddrChange();
					}
				}

				break;

			}
		}
	}

}

function UpdateMouse( pointer )
{
	if ( pointer == null )
		pointer = { b = 0 };

	mouse.click = pointer.b & ~mouse.old_b;

	if ( pointer.b == 1 )
	{
		++mouse.hold_time;
		if ( mouse.hold_time == mouse.hold_limit )
		{
			mouse.click = 1;

			mouse.hold_time = 0;
			mouse.hold_limit = 1; // fast
		}
	}
	else
	{
		mouse.hold_time = 0;
		mouse.hold_limit = 5; // slow
	}

	mouse.old_b = pointer.b;
}

function main( args )
{
	GCConsole.AutoYield( false );

	editor.snapBuffer = GCBuffer.Create();
	editor.tagBuffer = GCBuffer.Create();

	init();

	var retroPaused = null;
	var memorySize = 0;
	var retroActive = null;

	UITextCursor.Create();

	for ( ; ; )
	{
		var pointer = GCConsole.Pointer();

		GCKernel.Wait(0);

		if ( menu.IsOpen() == false )
		{
			var bChange = false;
			var newMemorySize = GCGameLink.MemorySize();

			if ( ( retroActive != GCGameLink.IsActive() ) || ( newMemorySize != memorySize ) )
			{
				bChange = true;
			}

			memorySize = newMemorySize;

			// State Change?
			if ( bChange )
			{
				editor.onStateChange();

				retroActive = GCGameLink.IsActive();
				retroPaused = null;

				if ( retroActive == true )
				{
					DrawScreen();
				}
				else
				{
					DrawBlankScreen();
				}
			}

			// Pause Change?
			if ( retroActive && retroPaused != GCGameLink.IsPaused() )
			{
				retroPaused = GCGameLink.IsPaused();
				DrawWindow();
			}
		}

		UpdateMouse( pointer );

		if ( menu.Update() )
		{
			var select = menu.Selection();
			if ( select )
			{
				if ( "id" in select.option )
				{
					onMenuSelect( select.option.id );
				}
			}
		}
		else
		{
			TickKey();

			if ( retroActive == true )
			{
				TickMouse( pointer );

				DrawRAM();
			}

			UITextCursor.Update();
		}
	}


	UITextCursor.Destroy();

	GCBuffer.Destroy( editor.snapBuffer );
	GCBuffer.Destroy( editor.tagBuffer );

	exit();
}


