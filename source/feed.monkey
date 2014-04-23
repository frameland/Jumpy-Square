Strict
Import vsat

Class LabelScene Extends VScene
	Field feed:LabelFeed = New LabelFeed
	
	Method OnInit:Void()
		feed.InitWithSizeAndFont(4, "lane_narrow")
		feed.position.Add(Vsat.ScreenWidth2, Vsat.ScreenHeight2)
	End
	
	Method OnUpdate:Void(dt:Float)
		feed.Update(dt)
		If KeyHit(KEY_SPACE) 
			feed.Push("Double Didge")
		End
		
	End

	Method OnRender:Void()
		feed.Render()
	End
	
End




Class LabelFeed Extends VRect
	
	Method New()
		Super.New(0, 0, 0, 0)
	End
	
	Method InitWithSizeAndFont:Void(initSize:Int, fontName:String)
		usedFont = FontCache.GetFont(fontName)
		lineHeight = usedFont.height
		maxItems = initSize
		items = New LabelFeedItem[maxItems]
		timeAlive = New Float[maxItems]
		For Local i:Int = 0 Until maxItems
			items[i] = New LabelFeedItem("")
			items[i].SetFont(usedFont)
		Next
	End
	
	Method Push:Void(itemText:String)
		tempLabels.AddLast(itemText)
	End
	
	Method Update:Void(dt:Float)
		If Not tempLabels.IsEmpty()
			nextPush -= dt
			If nextPush <= 0.0
				nextPush = PUSH_MINIMUM_TIME
				Local item:= tempLabels.RemoveLast()
				InitRealPush(item)
			End
		End
		
		For Local i:Int = 0 Until maxItems
			timeAlive[i] += dt
			If timeAlive[i] > 2.0
				items[i].color.Alpha -= dt * 1/PUSH_MINIMUM_TIME
				items[i].scale.y -= dt * 1/PUSH_MINIMUM_TIME
			End
		Next
		
		Local item:= items[0]
		If timeAlive[0] < PUSH_MINIMUM_TIME
			item.color.Alpha += dt * 1/PUSH_MINIMUM_TIME
			item.scale.y = item.color.Alpha
			If timeAlive[0] + dt >= PUSH_MINIMUM_TIME
				item.scale.y = 1.0
				item.color.Alpha = 1.0
			End
		End
		
	End
	
	Method Render:Void()
		PushMatrix()
			Translate(position.x, position.y)
			Local offsetY:Float = -lineHeight
			For Local i:Int = 0 Until maxItems
				PushMatrix()
					Translate(0, offsetY + items[0].scale.y * lineHeight)
					items[i].Render()
					offsetY += lineHeight
				PopMatrix()
			Next
		PopMatrix()
	End
	
	Method Clear:Void()
		For Local i:Int = 0 Until maxItems
			items[i].Text = ""
			items[i].scale.y = 0.0
			items[i].color.Alpha = 0.0
			timeAlive[i] = 0.0
		Next
	End
	
	
	Private
	Method InitRealPush:Void(itemText:String)
		Local lastItem:= items[maxItems-1]
		
		For Local i:Int = maxItems-1 To 1 Step -1
			items[i] = items[i-1]
			timeAlive[i] = timeAlive[i-1]
		Next
		
		items[0] = lastItem
		items[0].Text = itemText
		items[0].scale.y = 0.0
		items[0].color.Alpha = 0.0
		timeAlive[0] = 0.0
	End
	
	Private
	Field maxItems:Int = 5
	Field items:LabelFeedItem[]	
	Field timeAlive:Float[]
	
	Field tempLabels:StringList = New StringList
	Field nextPush:Float = PUSH_MINIMUM_TIME
	Const PUSH_MINIMUM_TIME:Float = 0.2
	
	
	Field usedFont:AngelFont
	Field lineHeight:Float
	
End



Class LabelFeedItem Extends VLabel
	
	Global medalIcon:Image
	
	Method New(text:String)
		Super.New(text)
		alignHorizontal = AngelFont.ALIGN_CENTER
		alignVertical = AngelFont.ALIGN_CENTER
		If Not medalIcon
			medalIcon = ImageCache.GetImage("medal.png", Image.MidHandle)
		End
	End
	
	Method SetFont:Void(font:AngelFont)
		Super.SetFont(font)
		UpdateMedalXPos()
	End
	
	Method Text:Void(text:String) Property
		Super.Text(text)
		UpdateMedalXPos()
	End
	
	Method Text:String() Property
		Return Super.Text
	End
	
	Method Draw:Void()
		If Self.Text
			#If TARGET <> "html5"
				Color.NewBlue.UseWithoutAlpha()
			#End
			DrawImage(medalIcon, medalX, 4)
			Super.Draw()
		End
	End
	
	Private
	Field medalX:Float
	
	Method UpdateMedalXPos:Void()
		medalX = -size.x * 0.55 - medalIcon.HandleX()
	End
	
End









