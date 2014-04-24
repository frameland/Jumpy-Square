Strict
Import vsat
Import medals

Class MedalItem Extends VRect
	
	Global font:AngelFont
	
	Method New(name:String, fileName:String)
		Super.New(0, 0, 0, 0)
		image = ImageCache.GetImage("medals/" + fileName, Image.MidHandle)
		Self.name = name
		times = Medals.HowManyOf(name)
		color.Set(Color.NewBlue)
		
		If Not font
			font = FontCache.GetFont("lane_narrow")
		End
	End
	
	Method Width:Float() Property
		Return image.Width()
	End
	
	Method Height:Float() Property
		Return image.Height() + font.TextHeight(times) * 1.5
	End
	
	Method Draw:Void()
		font.DrawText(times, 0, -image.Height()/2, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
		DrawImage(image, 0, font.TextHeight(times) * 1.5)
	End
	
	Method Name:String() Property
		Return name
	End
	
	Private
	Field image:Image
	Field name:String
	Field times:Int
End

