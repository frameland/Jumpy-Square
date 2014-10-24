Strict
Import vsat
Import brl.monkeystore

#rem
Class StoreScene Extends Scene Implements IStore
	
	Field store:VStore
	
	Method OnInit:Void()
		store = New VStore(Self, [""], ["SupporterMedal"])
	End
	
	Method OnUpdate:Void(dt:Float)
		If MouseHit()
			If PointInRect(MouseX(), MouseY(), 100, 100, 100, 40)
				store.Buy("SupporterMedal")
			End
			If PointInRect(MouseX(), MouseY(), 100, 300, 100, 40)
				store.Restore()
			End
		End
		
	End

	Method OnRender:Void()
		Color.White.Use()
		DrawRect(100, 100, 100, 40)
		DrawRect(100, 300, 100, 40)
		
		If store And store.IsOpen()
			Vsat.SystemFont.DrawText("Store is open", 4, 4)
			If store.IsBusy()
				Vsat.SystemFont.DrawText("Store is busy", 4, 20)
			End
		Else
			Vsat.SystemFont.DrawText("Store is closed", 4, 4)
		End
	End
	
	Method PurchaseComplete:Void(product:Product)
		Print product.Title
	End
	
	Method RestoreProducts:Void(products:Product[])
		Print "restoring" + products.Length + " products ..."
		For Local i:Int = 0 Until products.Length
			Print products[i].Title
		Next
	End
	
End
#end



'--------------------------------------------------------------------------
' * Interface
'--------------------------------------------------------------------------
Interface IStore
	Method PurchaseComplete:Void(product:Product)
	Method RestoreProducts:Void(products:Product[])
End


Class VStore Implements IOnOpenStoreComplete,IOnBuyProductComplete,IOnGetOwnedProductsComplete

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------
	Method New(callback:IStore, consumables:String[], nonConsumables:String[])
		Self.callback = callback
		store = New MonkeyStore
		If consumables.Length > 0
			store.AddProducts(consumables, 1)
		End
		If nonConsumables.Length > 0
			store.AddProducts(nonConsumables, 2)
		End
		store.OpenStoreAsync(Self)
	End
	

'--------------------------------------------------------------------------
' * Buy
'--------------------------------------------------------------------------	
	Method Buy:Void(productId:String)
		If CanBuy()
			Local product:Product = store.GetProduct(productId)
			If product
				store.BuyProductAsync(product, Self)
			Else
				'Print "Could not find product with id:" + productId
			End
		End
	End
	
	Method Restore:Void()
		If CanBuy()
			store.GetOwnedProductsAsync(Self)
		End
	End


'--------------------------------------------------------------------------
' * Helpers
'--------------------------------------------------------------------------
	Method CanBuy:Bool()
		Return store And store.IsOpen() And store.IsBusy() = False
	End
	
	Method IsBusy:Bool()
		Return store And store.IsBusy()
	End
	
	Method IsOpen:Bool()
		Return store And store.IsOpen()
	End
	
	Method LocalizedPrice:String(productId:String)
		If IsOpen()
			Local product:Product = store.GetProduct(productId)
			If product
				Return product.Price
			Else
				'Print "Could not find product with id:" + productId
			End
		End
		Return ""
	End
	
	
'--------------------------------------------------------------------------
' * Events for the interface
'--------------------------------------------------------------------------
	Method OnOpenStoreComplete:Void(result:Int)
		'Print "OpenStoreComplete, result="+result
		If result <> 0 
			'Print "Failed to open Monkey Store"
			store = Null
		End
	End
	
	Method OnBuyProductComplete:Void(result:Int, product:Product)
		'Print "BuyProductComplete, result="+result
		If result <> 0 Return
		If product.Type = 1 Or product.Type = 2
			callback.PurchaseComplete(product)
		End
	End
	
	Method OnGetOwnedProductsComplete:Void(result:Int, products:Product[])
		'Print "OnGetOwnedProductsComplete, result="+result
		If result <> 0 Return
		If Not products Return
		callback.RestoreProducts(products)
	End
	
	
'--------------------------------------------------------------------------
' Members
'--------------------------------------------------------------------------
	Private
	Field store:MonkeyStore
	Field callback:IStore
	
End








