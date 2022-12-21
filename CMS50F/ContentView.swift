//
//  ContentView.swift
//  CMS50F
//
//  Created by Oliver Epper on 15.12.22.
//

import SwiftUI

//extension cms50f_status_t {
//    static func ==(lhs: Self, rhs: cms50f_constants) -> Bool {
//        return lhs == rhs.rawValue
//    }
//
//    static func !=(lhs: Self, rhs: cms50f_constants) -> Bool {
//        return !(lhs == rhs)
//    }
//}


func print(_ cString: UnsafePointer<CChar>?) {
    if let cString {
        print(String(cString: cString))
    }
}

struct ContentView: View {
    var body: some View {
        Text("CMS50F")
        .padding()
        .onAppear {
            var device = cms50f_device_open("/dev/tty.usbserial-0001")
            
            if case let status = cms50f_device_destroy(&device), status != CMS50F_SUCCESS {
                return print(cms50f_strerror(status))
            }

            if case let status = cms50f_terminal_configure(device), status != CMS50F_SUCCESS {
                return print(cms50f_strerror(status))
            }
        }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
